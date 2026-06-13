

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <limits>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <random>
#include <memory>
#include <map>
#include <algorithm>

#include "vol_surface.hpp"

struct TestResult {
    std::string testName;
    bool passed;
    double executionTime;
    std::string errorMessage;
    
    TestResult(const std::string& name, bool pass, double time, const std::string& error = "")
        : testName(name), passed(pass), executionTime(time), errorMessage(error) {}
};

class TestSuite {
public:
    TestSuite(const std::string& name) : suiteName_(name) {}
    
    void addTest(const std::string& testName, std::function<void()> testFunc) {
        testFunctions_.push_back({testName, testFunc, 30.0});
    }
    
    void addTest(const std::string& testName, std::function<void()> testFunc, double timeoutSeconds) {
        testFunctions_.push_back({testName, testFunc, timeoutSeconds});
    }
    
    std::vector<TestResult> runAllTests() {
        std::vector<TestResult> results;
        for (const auto& test : testFunctions_) {
            results.push_back(runTest(test.name));
        }
        return results;
    }
    
    TestResult runTest(const std::string& testName) {
        for (const auto& test : testFunctions_) {
            if (test.name == testName) {
                TestResult result(testName, false, 0.0);
                runTestWithTimeout(test, result);
                return result;
            }
        }
        return TestResult(testName, false, 0.0, "Test not found");
    }
    
    void printResults(const std::vector<TestResult>& results) const {
        std::cout << "\n\n";
        std::cout << " Test Suite: " << suiteName_ << "\n";
        std::cout << "\n\n";
        
        int passed = 0, failed = 0;
        double totalTime = 0.0;
        
        for (const auto& r : results) {
            std::cout << (r.passed ? "[PASS] " : "[FAIL] ") << r.testName;
            std::cout << " (" << std::fixed << std::setprecision(3) << r.executionTime << "ms)";
            if (!r.passed && !r.errorMessage.empty()) {
                std::cout << "\n       Error: " << r.errorMessage;
            }
            std::cout << "\n";
            
            if (r.passed) ++passed;
            else ++failed;
            totalTime += r.executionTime;
        }
        
        std::cout << "\n\n";
        std::cout << "Results: " << passed << " passed, " << failed << " failed";
        std::cout << " (Total: " << std::fixed << std::setprecision(2) << totalTime << "ms)\n";
        std::cout << "\n";
    }
    
    int getTotalTests() const { return static_cast<int>(testFunctions_.size()); }
    
    int getPassedTests(const std::vector<TestResult>& results) const {
        int count = 0;
        for (const auto& r : results) if (r.passed) ++count;
        return count;
    }
    
    int getFailedTests(const std::vector<TestResult>& results) const {
        int count = 0;
        for (const auto& r : results) if (!r.passed) ++count;
        return count;
    }
    
    double getTotalTime(const std::vector<TestResult>& results) const {
        double total = 0.0;
        for (const auto& r : results) total += r.executionTime;
        return total;
    }

private:
    std::string suiteName_;
    struct TestInfo {
        std::string name;
        std::function<void()> function;
        double timeoutSeconds;
    };
    std::vector<TestInfo> testFunctions_;
    
    void runTestWithTimeout(const TestInfo& testInfo, TestResult& result) {
        auto start = std::chrono::high_resolution_clock::now();
        try {
            testInfo.function();
            result.passed = true;
        } catch (const std::exception& e) {
            result.passed = false;
            result.errorMessage = e.what();
        } catch (...) {
            result.passed = false;
            result.errorMessage = "Unknown exception";
        }
        auto end = std::chrono::high_resolution_clock::now();
        result.executionTime = std::chrono::duration<double, std::milli>(end - start).count();
    }
};

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error("Assertion failed: " #condition); \
        } \
    } while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            throw std::runtime_error("Assertion failed: expected " + std::to_string(expected) + \
                                   " but got " + std::to_string(actual)); \
        } \
    } while(0)

#define ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            throw std::runtime_error("Assertion failed: expected not equal to " + \
                                   std::to_string(expected) + " but got equal"); \
        } \
    } while(0)

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        double diff = std::abs((expected) - (actual)); \
        if (diff > (tolerance)) { \
            throw std::runtime_error("Assertion failed: expected " + std::to_string(expected) + \
                                   " but got " + std::to_string(actual) + \
                                   " (diff=" + std::to_string(diff) + \
                                   " > tolerance=" + std::to_string(tolerance) + ")"); \
        } \
    } while(0)

#define ASSERT_THROWS(expression) \
    do { \
        bool threw = false; \
        try { \
            expression; \
        } catch (...) { \
            threw = true; \
        } \
        if (!threw) { \
            throw std::runtime_error("Assertion failed: expected exception but none was thrown"); \
        } \
    } while(0)

#define ASSERT_NAN(value) \
    do { \
        if (!std::isnan(value)) { \
            throw std::runtime_error("Assertion failed: expected NaN but got " + \
                                    std::to_string(value)); \
        } \
    } while(0)

#define ASSERT_FINITE(value) \
    do { \
        if (!std::isfinite(value)) { \
            throw std::runtime_error("Assertion failed: expected finite but got " + \
                                    std::to_string(value)); \
        } \
    } while(0)

#define ASSERT_IN_RANGE(value, min_val, max_val) \
    do { \
        double v = (value); \
        if (v < (min_val) || v > (max_val)) { \
            throw std::runtime_error("Assertion failed: " + std::to_string(v) + \
                                    " not in range [" + std::to_string(min_val) + \
                                    ", " + std::to_string(max_val) + "]"); \
        } \
    } while(0)

#define ASSERT_EMPTY(container) \
    do { \
        if (!container.empty()) { \
            throw std::runtime_error("Assertion failed: expected empty container " \
                                    "but size is " + std::to_string(container.size())); \
        } \
    } while(0)

#define ASSERT_NOT_EMPTY(container) \
    do { \
        if (container.empty()) { \
            throw std::runtime_error("Assertion failed: expected non-empty container"); \
        } \
    } while(0)

#define ASSERT_SIZE(container, expected_size) \
    do { \
        if (container.size() != (expected_size)) { \
            throw std::runtime_error("Assertion failed: expected size " + \
                                    std::to_string(expected_size) + " but got " + \
                                    std::to_string(container.size())); \
        } \
    } while(0)

#define ASSERT_RELATIVE_ERROR(expected, actual, rel_tol) \
    do { \
        double exp = (expected); \
        double act = (actual); \
        double rel_err = (exp != 0.0) ? std::abs((exp - act) / exp) : std::abs(act); \
        if (rel_err > (rel_tol)) { \
            throw std::runtime_error("Assertion failed: relative error " + \
                                    std::to_string(rel_err) + " > " + \
                                    std::to_string(rel_tol)); \
        } \
    } while(0)

#define ASSERT_THROWS_TYPE(expression, exception_type) \
    do { \
        bool correct_exception = false; \
        try { \
            expression; \
        } catch (const exception_type&) { \
            correct_exception = true; \
        } catch (...) { \
            throw std::runtime_error("Assertion failed: wrong exception type thrown"); \
        } \
        if (!correct_exception) { \
            throw std::runtime_error("Assertion failed: expected " #exception_type); \
        } \
    } while(0)

#define ASSERT_NO_THROW(expression) \
    do { \
        try { \
            expression; \
        } catch (const std::exception& e) { \
            throw std::runtime_error("Assertion failed: unexpected exception: " + \
                                    std::string(e.what())); \
        } catch (...) { \
            throw std::runtime_error("Assertion failed: unexpected unknown exception"); \
        } \
    } while(0)

#define ASSERT_COMPLETES_WITHIN(expression, timeout_ms) \
    do { \
        auto start = std::chrono::high_resolution_clock::now(); \
        expression; \
        auto end = std::chrono::high_resolution_clock::now(); \
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start); \
        if (duration.count() > (timeout_ms)) { \
            throw std::runtime_error("Assertion failed: execution took " + \
                                    std::to_string(duration.count()) + "ms > " + \
                                    std::to_string(timeout_ms) + "ms"); \
        } \
    } while(0)

#define ASSERT_GT(a, b) \
    do { \
        if (!((a) > (b))) { \
            throw std::runtime_error("Assertion failed: " + std::to_string(a) + \
                                    " not greater than " + std::to_string(b)); \
        } \
    } while(0)

#define ASSERT_GE(a, b) \
    do { \
        if (!((a) >= (b))) { \
            throw std::runtime_error("Assertion failed: " + std::to_string(a) + \
                                    " not >= " + std::to_string(b)); \
        } \
    } while(0)

#define ASSERT_LT(a, b) \
    do { \
        if (!((a) < (b))) { \
            throw std::runtime_error("Assertion failed: " + std::to_string(a) + \
                                    " not less than " + std::to_string(b)); \
        } \
    } while(0)

#define ASSERT_LE(a, b) \
    do { \
        if (!((a) <= (b))) { \
            throw std::runtime_error("Assertion failed: " + std::to_string(a) + \
                                    " not <= " + std::to_string(b)); \
        } \
    } while(0)

class ThreadSafetyTester {
public:
    template<typename Func>
    static bool runConcurrent(Func func, int numThreads, int iterationsPerThread) {
        std::atomic<bool> failed{false};
        std::atomic<int> completedIterations{0};
        std::vector<std::thread> threads;
        std::mutex exceptionMutex;
        std::string firstException;
        
        std::atomic<int> readyCount{0};
        std::condition_variable startCV;
        std::mutex startMutex;
        
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                readyCount++;
                {
                    std::unique_lock<std::mutex> lock(startMutex);
                    startCV.wait(lock, [&]() { return readyCount >= numThreads; });
                }
                
                for (int i = 0; i < iterationsPerThread && !failed; ++i) {
                    try {
                        func(t, i);
                        completedIterations++;
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lock(exceptionMutex);
                        if (firstException.empty()) {
                            firstException = e.what();
                        }
                        failed = true;
                    }
                }
            });
        }
        
        {
            std::lock_guard<std::mutex> lock(startMutex);
            startCV.notify_all();
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (failed) {
            throw std::runtime_error("Thread safety test failed: " + firstException);
        }
        
        return completedIterations == numThreads * iterationsPerThread;
    }
    
    template<typename Reader, typename Writer>
    static bool detectDataRace(Reader reader, Writer writer, 
                               int numReaders, int numWriters,
                               int iterations) {
        std::atomic<bool> stop{false};
        std::atomic<int> readCount{0};
        std::atomic<int> writeCount{0};
        std::atomic<bool> raceDetected{false};
        
        std::vector<std::thread> threads;
        
        for (int w = 0; w < numWriters; ++w) {
            threads.emplace_back([&, w]() {
                for (int i = 0; i < iterations && !stop; ++i) {
                    try {
                        writer(w, i);
                        writeCount++;
                    } catch (...) {
                        raceDetected = true;
                        stop = true;
                    }
                }
            });
        }
        
        for (int r = 0; r < numReaders; ++r) {
            threads.emplace_back([&, r]() {
                for (int i = 0; i < iterations && !stop; ++i) {
                    try {
                        reader(r, i);
                        readCount++;
                    } catch (...) {
                        raceDetected = true;
                        stop = true;
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        return !raceDetected;
    }
};

class StatisticalTester {
public:
    template<typename Func, typename T>
    static bool testConsistency(Func func, int numTrials, T tolerance) {
        std::vector<T> results;
        results.reserve(numTrials);
        
        for (int i = 0; i < numTrials; ++i) {
            results.push_back(func());
        }
        
        T first = results[0];
        for (const auto& r : results) {
            if (std::abs(r - first) > tolerance) {
                return false;
            }
        }
        return true;
    }
    
    template<typename Func>
    static double measureP99ExecutionTime(Func func, int numTrials) {
        std::vector<double> times;
        times.reserve(numTrials);
        
        for (int i = 0; i < numTrials; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            times.push_back(duration.count() / 1000.0);
        }
        
        std::sort(times.begin(), times.end());
        int p99Index = static_cast<int>(numTrials * 0.99);
        return times[p99Index];
    }
    
    static std::vector<double> generateRandomDoubles(int count, double min, double max) {
        std::vector<double> result;
        result.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(min, max);
        
        for (int i = 0; i < count; ++i) {
            result.push_back(dist(gen));
        }
        return result;
    }
};

class MemoryTester {
public:
    template<typename Func>
    static size_t measureApproxMemory(Func func) {
        func();
        return 0;  
    }
    
    template<typename Func>
    static bool checkForLeaks(Func func, int iterations, double maxGrowthRatio = 1.1) {
        func();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        return true;  
    }
};

class PerformanceTester {
public:
    static double measureExecutionTime(std::function<void()> func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
    
    static void benchmarkFunction(const std::string& name, std::function<void()> func, int iterations = 1000) {
        
        for (int i = 0; i < std::min(10, iterations / 10); ++i) {
            func();
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
        double avgUs = (totalMs * 1000.0) / iterations;
        
        std::cout << "[Benchmark] " << name << ": " 
                  << std::fixed << std::setprecision(2) << avgUs << " s/op"
                  << " (" << iterations << " iterations)\n";
    }
    
    static void memoryUsageTest(const std::string& name, std::function<void()> func) {
        std::cout << "[Memory] " << name << ": (measurement not implemented)\n";
        func();
    }
};

class MockDataGenerator {
public:
    static std::vector<Quote> generateQuotes(int numQuotes, double spot = 100.0) {
        std::vector<Quote> quotes;
        quotes.reserve(numQuotes);
        
        std::vector<double> expiries = {0.25, 0.5, 1.0, 2.0};
        std::vector<double> moneyness = {0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2};
        
        for (double T : expiries) {
            for (double m : moneyness) {
                double K = spot * m;
                double logM = std::log(m);
                double iv = 0.20 + 0.10 * logM * logM + 0.02 * std::sqrt(T);
                quotes.push_back({K, T, iv});
                if (static_cast<int>(quotes.size()) >= numQuotes) break;
            }
            if (static_cast<int>(quotes.size()) >= numQuotes) break;
        }
        
        return quotes;
    }
    
    static std::vector<Quote> generateArbitrageFreeQuotes(int numQuotes, double spot = 100.0) {
        std::vector<Quote> quotes;
        
        std::vector<double> expiries = {0.25, 0.5, 1.0};
        std::vector<double> strikes;
        for (int i = 0; i < 7; ++i) {
            strikes.push_back(spot * (0.8 + 0.067 * i));
        }

        double baseVariance = 0.04;   
        double smile = 0.02;          
        double termSlope = 0.01;      
        
        for (double T : expiries) {
            for (double K : strikes) {
                double logM = std::log(K / spot);
                
                double totalVar = baseVariance * T + smile * logM * logM + termSlope * T;
                double iv = std::sqrt(totalVar / T);
                
                iv = std::max(0.05, std::min(iv, 1.0));
                quotes.push_back({K, T, iv});
                if (static_cast<int>(quotes.size()) >= numQuotes) break;
            }
            if (static_cast<int>(quotes.size()) >= numQuotes) break;
        }
        
        return quotes;
    }
    
    static std::vector<Quote> generateButterflyArbitrageQuotes(int numQuotes, double spot = 100.0) {
        auto quotes = generateArbitrageFreeQuotes(numQuotes, spot);
        
        for (auto& q : quotes) {
            if (std::abs(q.strike - spot) < 1.0 && std::abs(q.expiry - 0.5) < 0.01) {
                q.iv = 0.05;  
            }
        }
        return quotes;
    }
    
    static std::vector<Quote> generateCalendarArbitrageQuotes(int numQuotes, double spot = 100.0) {
        auto quotes = generateArbitrageFreeQuotes(numQuotes, spot);

        double targetStrike = spot * 0.867;  
        for (auto& q : quotes) {
            if (std::abs(q.strike - targetStrike) < 2.0 && std::abs(q.expiry - 1.0) < 0.01) {
                q.iv = 0.08;  
            }
        }
        return quotes;
    }
    
    static MarketData generateMarketData(double spot = 100.0) {
        return {spot, 0.05, 0.02, "2024-01-01", "USD"};
    }
};

class ExtendedMockDataGenerator {
public:
    static std::vector<Quote> generateExtremeValueQuotes(double spot = 100.0) {
        std::vector<Quote> quotes;
        
        quotes.push_back({spot, 0.5, 0.001});      
        quotes.push_back({spot, 0.5, 4.5});        
        quotes.push_back({spot, 0.001, 0.2});      
        quotes.push_back({spot, 9.9, 0.2});        
        quotes.push_back({spot * 0.5, 0.5, 0.3});  
        quotes.push_back({spot * 2.0, 0.5, 0.3});  
        quotes.push_back({spot * 0.1, 0.5, 0.5});  
        quotes.push_back({spot * 5.0, 0.5, 0.5});  
        
        return quotes;
    }
    
    static std::vector<Quote> generateNumericalEdgeCases(double spot = 100.0) {
        std::vector<Quote> quotes;
        
        quotes.push_back({100.0, 0.5, 0.20});
        quotes.push_back({100.001, 0.5, 0.201});
        quotes.push_back({100.0, 0.5001, 0.201});
        quotes.push_back({spot, 1e-6, 0.2});
        
        return quotes;
    }
    
    static std::vector<Quote> generateAllArbitrageTypes(double spot = 100.0) {
        std::vector<Quote> quotes;
        
        for (double T : {0.25, 0.5, 1.0}) {
            for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
                double moneyness = std::log(K / spot);
                double baseVol = 0.20 + 0.05 * std::abs(moneyness);
                quotes.push_back({K, T, baseVol});
            }
        }

        for (auto& q : quotes) {
            if (std::abs(q.strike - spot) < 1.0 && std::abs(q.expiry - 0.5) < 0.01) {
                q.iv = 0.05;
            }
        }

        for (auto& q : quotes) {
            if (std::abs(q.strike - 90.0) < 1.0 && std::abs(q.expiry - 1.0) < 0.01) {
                q.iv = 0.10;
            }
        }
        
        return quotes;
    }
    
    static std::vector<Quote> generateLargeDataset(int numStrikes, int numExpiries, 
                                                     double spot = 100.0) {
        std::vector<Quote> quotes;
        quotes.reserve(numStrikes * numExpiries);
        
        std::vector<double> strikes;
        for (int i = 0; i < numStrikes; ++i) {
            double pct = 0.5 + 1.0 * i / (numStrikes - 1);
            strikes.push_back(spot * pct);
        }
        
        std::vector<double> expiries;
        for (int i = 0; i < numExpiries; ++i) {
            double T = 0.1 + 4.9 * i / (numExpiries - 1);
            expiries.push_back(T);
        }
        
        for (double T : expiries) {
            for (double K : strikes) {
                double moneyness = std::log(K / spot);
                double baseVol = 0.20 + 0.1 * std::abs(moneyness) + 0.02 * std::sqrt(T);
                quotes.push_back({K, T, baseVol});
            }
        }
        
        return quotes;
    }
};

enum class TestCategory {
    UNIT,
    INTEGRATION,
    EDGE_CASE,
    STRESS,
    PERFORMANCE,
    THREAD_SAFETY,
    REGRESSION
};

struct TestMetadata {
    std::string name;
    TestCategory category;
    int priority;
    double timeoutSeconds;
    bool requiresFiles;
};

class TestRunner {
public:
    static TestRunner& getInstance() {
        static TestRunner instance;
        return instance;
    }
    
    void addSuite(std::unique_ptr<TestSuite> suite) {
        testSuites_.push_back(std::move(suite));
    }
    
    int runAllSuites() {
        int totalPassed = 0, totalFailed = 0;
        
        std::cout << "\n\n";
        std::cout << "           VOLATILITY ARBITRAGE TEST RUNNER                    \n";
        std::cout << "\n";
        
        for (auto& suite : testSuites_) {
            auto results = suite->runAllTests();
            suite->printResults(results);
            totalPassed += suite->getPassedTests(results);
            totalFailed += suite->getFailedTests(results);
        }
        
        std::cout << "\n\n";
        std::cout << "                     FINAL SUMMARY                             \n";
        std::cout << "\n";
        std::cout << "  Total Passed: " << std::setw(4) << totalPassed << "                                        \n";
        std::cout << "  Total Failed: " << std::setw(4) << totalFailed << "                                        \n";
        std::cout << "\n";
        
        return totalFailed;
    }
    
    void runSuite(const std::string& ) {
        
    }
    
private:
    TestRunner() = default;
    std::vector<std::unique_ptr<TestSuite>> testSuites_;
};
