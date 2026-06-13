

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "config_manager.hpp"
#include "logger.hpp"
#include "vol_api.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <future>
#include <chrono>

void test_vol_surface_concurrent_reads() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(20, 100.0);
    VolSurface surface(quotes, marketData);
    
    const int numThreads = 8;
    const int readsPerThread = 1000;
    std::atomic<int> successCount{0};
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < readsPerThread && !failed; ++i) {
                try {
                    
                    double K = 80.0 + (t * 5.0) + (i % 10);
                    double T = 0.25 + (i % 4) * 0.25;
                    double vol = surface.impliedVol(K, T);
                    
                    if (std::isfinite(vol) && vol > 0.0) {
                        successCount++;
                    }
                } catch (const std::exception&) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
    ASSERT_TRUE(successCount.load() > numThreads * readsPerThread / 2);
}

void test_vol_surface_concurrent_pricing() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(15, 100.0);
    VolSurface surface(quotes, marketData);
    
    const int numThreads = 4;
    const int pricesPerThread = 500;
    std::atomic<int> validPrices{0};
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < pricesPerThread && !failed; ++i) {
                try {
                    double K = 90.0 + (i % 20);
                    double T = 0.5 + (i % 4) * 0.25;
                    double callPrice = surface.callPrice(K, T);
                    double putPrice = surface.putPrice(K, T);
                    
                    if (std::isfinite(callPrice) && callPrice >= 0.0 &&
                        std::isfinite(putPrice) && putPrice >= 0.0) {
                        validPrices++;
                    }
                } catch (const std::exception&) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
    ASSERT_TRUE(validPrices.load() > 0);
}

void test_arbitrage_detector_concurrent_detection() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(15, 100.0);
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    const int numThreads = 4;
    const int detectionsPerThread = 100;
    std::atomic<int> successCount{0};
    std::atomic<bool> failed{false};
    std::mutex resultsMutex;
    std::vector<size_t> violationCounts;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < detectionsPerThread && !failed; ++i) {
                try {
                    auto violations = detector.detect();
                    {
                        std::lock_guard<std::mutex> lock(resultsMutex);
                        violationCounts.push_back(violations.size());
                    }
                    successCount++;
                } catch (const std::exception&) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
    ASSERT_EQ(successCount.load(), numThreads * detectionsPerThread);

    if (!violationCounts.empty()) {
        size_t firstCount = violationCounts[0];
        for (size_t count : violationCounts) {
            ASSERT_EQ(count, firstCount);
        }
    }
}

void test_multiple_detectors_parallel() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    
    const int numDetectors = 4;
    std::vector<std::future<bool>> futures;
    
    for (int d = 0; d < numDetectors; ++d) {
        futures.push_back(std::async(std::launch::async, [&, d]() {
            try {
                
                auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(
                    10 + d * 2, 100.0);
                VolSurface surface(quotes, marketData);
                ArbitrageDetector detector(surface);
                
                auto violations = detector.detect();
                return true;
            } catch (...) {
                return false;
            }
        }));
    }
    
    int successCount = 0;
    for (auto& f : futures) {
        if (f.get()) {
            successCount++;
        }
    }
    
    ASSERT_EQ(successCount, numDetectors);
}

void test_qp_solver_parallel_solves() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    
    const int numSolvers = 4;
    std::vector<std::future<bool>> futures;
    
    for (int s = 0; s < numSolvers; ++s) {
        futures.push_back(std::async(std::launch::async, [&]() {
            try {
                auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(12, 100.0);
                VolSurface surface(quotes, marketData);
                QPSolver solver;
solver.setup(surface);
                
                auto result = solver.solve(surface);
                return result.success;
            } catch (...) {
                return false;
            }
        }));
    }
    
    int successCount = 0;
    for (auto& f : futures) {
        if (f.get()) {
            successCount++;
        }
    }
    
    ASSERT_TRUE(successCount >= numSolvers / 2);
}

void test_qp_solver_deterministic_under_concurrency() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(10, 100.0);

    const int numSolves = 4;
    std::vector<std::future<double>> futures;
    
    for (int s = 0; s < numSolves; ++s) {
        futures.push_back(std::async(std::launch::async, [&]() {
            VolSurface surface(quotes, marketData);
            QPSolver solver;
solver.setup(surface);
            auto result = solver.solve(surface);
            return result.objectiveValue;
        }));
    }
    
    std::vector<double> objectives;
    for (auto& f : futures) {
        objectives.push_back(f.get());
    }

    double first = objectives[0];
    for (double obj : objectives) {
        ASSERT_NEAR(obj, first, 1e-6);
    }
}

void test_config_manager_concurrent_access() {
    auto& config = ConfigManager::getInstance();
    
    const int numThreads = 4;
    const int opsPerThread = 200;
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < opsPerThread && !failed; ++i) {
                try {
                    if (i % 2 == 0) {
                        
                        auto val = config.get<double>("test.value", 0.0);
                        (void)val;
                    } else {
                        
                        config.set<double>("test.value", static_cast<double>(t * i));
                    }
                } catch (const std::exception&) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
}

void test_config_manager_concurrent_sections() {
    auto& config = ConfigManager::getInstance();
    
    const int numThreads = 4;
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            try {
                std::string section = "thread_" + std::to_string(t);
                
                for (int i = 0; i < 100 && !failed; ++i) {
                    config.set<int>(section + ".counter", i);
                    auto val = config.get<int>(section + ".counter", -1);

                    if (val < 0 || val > 99) {
                        failed = true;
                    }
                }
            } catch (const std::exception&) {
                failed = true;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
}

void test_logger_concurrent_writes() {
    auto& logger = Logger::getInstance();
    
    const int numThreads = 8;
    const int logsPerThread = 100;
    std::atomic<int> logCount{0};
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < logsPerThread && !failed; ++i) {
                try {
                    logger.log(LogLevel::INFO, 
                        "Thread " + std::to_string(t) + " message " + std::to_string(i));
                    logCount++;
                } catch (const std::exception&) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
    ASSERT_EQ(logCount.load(), numThreads * logsPerThread);
}

void test_logger_mixed_levels_concurrent() {
    auto& logger = Logger::getInstance();
    
    const int numThreads = 4;
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            try {
                for (int i = 0; i < 50 && !failed; ++i) {
                    switch (i % 4) {
                        case 0:
                            logger.debug("Debug from thread " + std::to_string(t));
                            break;
                        case 1:
                            logger.info("Info from thread " + std::to_string(t));
                            break;
                        case 2:
                            logger.warning("Warning from thread " + std::to_string(t));
                            break;
                        case 3:
                            logger.error("Error from thread " + std::to_string(t));
                            break;
                    }
                }
            } catch (const std::exception&) {
                failed = true;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
}

void test_cached_surface_concurrent_reads() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(20, 100.0);
    CachedVolSurface cachedSurface(quotes, marketData);
    
    const int numThreads = 8;
    const int readsPerThread = 500;
    std::atomic<int> successCount{0};
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < readsPerThread && !failed; ++i) {
                try {
                    
                    double K = 90.0 + (i % 10) * 2.0;
                    double T = 0.25 + (i % 4) * 0.25;
                    
                    double vol = cachedSurface.impliedVol(K, T);
                    
                    if (std::isfinite(vol) && vol > 0.0) {
                        successCount++;
                    }
                } catch (const std::exception&) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
    ASSERT_TRUE(successCount.load() > numThreads * readsPerThread / 2);

    double hitRate = cachedSurface.getCacheHitRate();
    ASSERT_TRUE(hitRate > 0.5);
}

void test_cached_surface_cache_operations_concurrent() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(15, 100.0);
    CachedVolSurface cachedSurface(quotes, marketData);
    
    std::atomic<bool> failed{false};
    std::atomic<bool> stopReaders{false};

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            while (!stopReaders && !failed) {
                try {
                    double K = 95.0 + (rand() % 10);
                    double T = 0.5 + (rand() % 4) * 0.25;
                    cachedSurface.impliedVol(K, T);
                } catch (...) {
                    failed = true;
                }
            }
        });
    }

    std::thread manager([&]() {
        for (int i = 0; i < 10 && !failed; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            try {
                cachedSurface.clearCache();
                cachedSurface.resetPerformanceMetrics();
            } catch (...) {
                failed = true;
            }
        }
        stopReaders = true;
    });
    
    manager.join();
    for (auto& t : readers) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
}

void test_high_contention_scenario() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0);
    VolSurface surface(quotes, marketData);
    
    const int numThreads = 16;
    const int opsPerThread = 200;
    std::atomic<int> successOps{0};
    std::atomic<bool> failed{false};
    
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < opsPerThread && !failed; ++i) {
                try {
                    
                    switch (i % 3) {
                        case 0:
                            surface.impliedVol(100.0, 0.5);
                            break;
                        case 1:
                            surface.callPrice(100.0, 0.5);
                            break;
                        case 2: {
                            auto strikes = surface.strikes();
                            auto expiries = surface.expiries();
                            (void)strikes;
                            (void)expiries;
                            break;
                        }
                    }
                    successOps++;
                } catch (...) {
                    failed = true;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_FALSE(failed.load());
    ASSERT_EQ(successOps.load(), numThreads * opsPerThread);
}

void registerThreadSafetyTests() {
    auto& runner = TestRunner::getInstance();
    
    auto suite = std::make_unique<TestSuite>("ThreadSafety");

    suite->addTest("VolSurfaceConcurrentReads", test_vol_surface_concurrent_reads);
    suite->addTest("VolSurfaceConcurrentPricing", test_vol_surface_concurrent_pricing);

    suite->addTest("ArbitrageDetectorConcurrentDetection", test_arbitrage_detector_concurrent_detection);
    suite->addTest("MultipleDetectorsParallel", test_multiple_detectors_parallel);

    suite->addTest("QPSolverParallelSolves", test_qp_solver_parallel_solves);
    suite->addTest("QPSolverDeterministicUnderConcurrency", test_qp_solver_deterministic_under_concurrency);

    suite->addTest("ConfigManagerConcurrentAccess", test_config_manager_concurrent_access);
    suite->addTest("ConfigManagerConcurrentSections", test_config_manager_concurrent_sections);

    suite->addTest("LoggerConcurrentWrites", test_logger_concurrent_writes);
    suite->addTest("LoggerMixedLevelsConcurrent", test_logger_mixed_levels_concurrent);

    suite->addTest("CachedSurfaceConcurrentReads", test_cached_surface_concurrent_reads);
    suite->addTest("CachedSurfaceCacheOperationsConcurrent", test_cached_surface_cache_operations_concurrent);

    suite->addTest("HighContentionScenario", test_high_contention_scenario, 30.0);
    
    runner.addSuite(std::move(suite));
}
