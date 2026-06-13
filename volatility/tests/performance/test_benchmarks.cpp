

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include <chrono>
#include <cmath>

void test_perf_surface_construction_small() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(15, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        VolSurface surface(quotes, marketData);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 100.0;

    ASSERT_TRUE(avgUs < 10000);  
    
    std::cout << "  Avg surface construction (15 quotes): " << avgUs << " s\n";
}

void test_perf_surface_construction_medium() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(100, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 50; ++i) {
        VolSurface surface(quotes, marketData);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 50.0;
    
    ASSERT_TRUE(avgUs < 50000);  
    
    std::cout << "  Avg surface construction (100 quotes): " << avgUs << " s\n";
}

void test_perf_interpolation_single() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
        surface.impliedVol(100.0, 0.5);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgNs = (duration.count() * 1000.0) / 100000.0;
    
    ASSERT_TRUE(avgNs < 10000);  
    
    std::cout << "  Avg interpolation time: " << avgNs << " ns\n";
}

void test_perf_interpolation_random() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        double K = 80.0 + (i % 40);
        double T = 0.25 + (i % 4) * 0.25;
        surface.impliedVol(K, T);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 10000.0;
    
    ASSERT_TRUE(avgUs < 100);  
    
    std::cout << "  Avg random interpolation time: " << avgUs << " s\n";
}

void test_perf_call_pricing() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        surface.callPrice(100.0, 1.0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 10000.0;
    
    ASSERT_TRUE(avgUs < 100);  
    
    std::cout << "  Avg call pricing time: " << avgUs << " s\n";
}

void test_perf_put_pricing() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        surface.putPrice(100.0, 1.0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 10000.0;
    
    ASSERT_TRUE(avgUs < 100);  
    
    std::cout << "  Avg put pricing time: " << avgUs << " s\n";
}

void test_perf_arbitrage_detection_small() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(15, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        ArbitrageDetector detector(surface);
        detector.detect();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 100.0;
    
    ASSERT_TRUE(avgUs < 50000);  
    
    std::cout << "  Avg arbitrage detection (15 quotes): " << avgUs << " s\n";
}

void test_perf_arbitrage_detection_medium() {
    auto quotes = ExtendedMockDataGenerator::generateLargeDataset(10, 5, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 50; ++i) {
        ArbitrageDetector detector(surface);
        detector.detect();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgUs = duration.count() / 50.0;
    
    ASSERT_TRUE(avgUs < 100000);  
    
    std::cout << "  Avg arbitrage detection (50 quotes): " << avgUs << " s\n";
}

void test_perf_qp_solver_small() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(15, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 20; ++i) {
        QPSolver solver;
solver.setup(surface);
        solver.solve(surface);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avgMs = duration.count() / 20.0;
    
    ASSERT_TRUE(avgMs < 2000);  
    
    std::cout << "  Avg QP solve time (15 quotes): " << avgMs << " ms\n";
}

void test_perf_qp_solver_medium() {
    auto quotes = ExtendedMockDataGenerator::generateLargeDataset(10, 5, 100.0);
    
    for (auto& q : quotes) {
        if (std::abs(q.strike - 100.0) < 5.0 && std::abs(q.expiry - 1.0) < 0.1) {
            q.iv *= 0.5;  
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; ++i) {
        QPSolver solver;
solver.setup(surface);
        solver.solve(surface);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avgMs = duration.count() / 10.0;
    
    ASSERT_TRUE(avgMs < 5000);  
    
    std::cout << "  Avg QP solve time (50 quotes): " << avgMs << " ms\n";
}

void test_perf_full_pipeline() {
    auto start = std::chrono::high_resolution_clock::now();
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    for (int i = 0; i < 5; ++i) {
        auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
        VolSurface surface(quotes, marketData);
        
        ArbitrageDetector detector(surface);
        auto violations = detector.detect();
        
        if (!violations.empty()) {
            QPSolver solver;
solver.setup(surface);
            QPResult result = solver.solve(surface);
            
            if (result.success) {
                VolSurface corrected = solver.buildCorrectedSurface(surface, result);
                corrected.impliedVol(100.0, 0.5);
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avgMs = duration.count() / 5.0;
    
    ASSERT_TRUE(avgMs < 10000);  
    
    std::cout << "  Avg full pipeline time: " << avgMs << " ms\n";
}

void test_perf_throughput() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    VolSurface surface(quotes, marketData);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int operations = 0;
    while (true) {
        surface.impliedVol(100.0, 0.5);
        surface.callPrice(100.0, 0.5);
        surface.putPrice(100.0, 0.5);
        operations += 3;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 1000) break;  
    }

    ASSERT_TRUE(operations > 1000);
    
    std::cout << "  Throughput: " << operations << " ops/sec\n";
}

std::unique_ptr<TestSuite> createPerformanceBenchmarkTestSuite() {
    auto suite = std::make_unique<TestSuite>("Performance Benchmarks");

    suite->addTest("Perf Surface Construction Small", test_perf_surface_construction_small, 30.0);
    suite->addTest("Perf Surface Construction Medium", test_perf_surface_construction_medium, 60.0);

    suite->addTest("Perf Interpolation Single", test_perf_interpolation_single, 30.0);
    suite->addTest("Perf Interpolation Random", test_perf_interpolation_random, 30.0);

    suite->addTest("Perf Call Pricing", test_perf_call_pricing, 30.0);
    suite->addTest("Perf Put Pricing", test_perf_put_pricing, 30.0);

    suite->addTest("Perf Arbitrage Detection Small", test_perf_arbitrage_detection_small, 30.0);
    suite->addTest("Perf Arbitrage Detection Medium", test_perf_arbitrage_detection_medium, 60.0);

    suite->addTest("Perf QP Solver Small", test_perf_qp_solver_small, 60.0);
    suite->addTest("Perf QP Solver Medium", test_perf_qp_solver_medium, 120.0);

    suite->addTest("Perf Full Pipeline", test_perf_full_pipeline, 120.0);
    suite->addTest("Perf Throughput", test_perf_throughput, 10.0);
    
    return suite;
}

void registerBenchmarkTests() {
    auto& runner = TestRunner::getInstance();
    runner.addSuite(createPerformanceBenchmarkTestSuite());
}
