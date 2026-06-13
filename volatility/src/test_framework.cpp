#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "data_handler.hpp"
#include <thread>
#include <future>
#include <random>
#include <algorithm>
#include <cmath>

TestSuite::TestSuite(const std::string& name) : suiteName_(name) {}

void TestSuite::addTest(const std::string& testName, std::function<void()> testFunc) {
    addTest(testName, testFunc, 30.0); 
}

void TestSuite::addTest(const std::string& testName, std::function<void()> testFunc, double timeoutSeconds) {
    testFunctions_.push_back({testName, testFunc, timeoutSeconds});
}

std::vector<TestResult> TestSuite::runAllTests() {
    std::vector<TestResult> results;
    
    std::cout << "\n=== Running Test Suite: " << suiteName_ << " ===" << std::endl;
    
    for (const auto& testInfo : testFunctions_) {
        TestResult result(testInfo.name, false, 0.0);
        runTestWithTimeout(testInfo, result);
        results.push_back(result);
        
        std::cout << "[" << (result.passed ? "PASS" : "FAIL") << "] " 
                  << testInfo.name << " (" << std::fixed << std::setprecision(3) 
                  << result.executionTime << "s)" << std::endl;
        
        if (!result.passed) {
            std::cout << "    Error: " << result.errorMessage << std::endl;
        }
    }
    
    return results;
}

TestResult TestSuite::runTest(const std::string& testName) {
    for (const auto& testInfo : testFunctions_) {
        if (testInfo.name == testName) {
            TestResult result(testInfo.name, false, 0.0);
            runTestWithTimeout(testInfo, result);
            return result;
        }
    }
    throw std::runtime_error("Test not found: " + testName);
}

void TestSuite::runTestWithTimeout(const TestInfo& testInfo, TestResult& result) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        
        auto future = std::async(std::launch::async, testInfo.function);
        
        if (future.wait_for(std::chrono::duration<double>(testInfo.timeoutSeconds)) == std::future_status::timeout) {
            result.passed = false;
            result.errorMessage = "Test timed out after " + std::to_string(testInfo.timeoutSeconds) + " seconds";
        } else {
            future.get(); 
            result.passed = true;
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = e.what();
    } catch (...) {
        result.passed = false;
        result.errorMessage = "Unknown exception";
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.executionTime = duration.count() / 1000.0;
}

void TestSuite::printResults(const std::vector<TestResult>& results) const {
    int passed = getPassedTests(results);
    int failed = getFailedTests(results);
    double totalTime = getTotalTime(results);
    
    std::cout << "\n=== Test Suite Results: " << suiteName_ << " ===" << std::endl;
    std::cout << "Total: " << results.size() << ", Passed: " << passed 
              << ", Failed: " << failed << std::endl;
    std::cout << "Total time: " << std::fixed << std::setprecision(3) 
              << totalTime << "s" << std::endl;
    std::cout << "Success rate: " << std::setprecision(1) 
              << (100.0 * passed / results.size()) << "%" << std::endl;
}

int TestSuite::getPassedTests(const std::vector<TestResult>& results) const {
    return std::count_if(results.begin(), results.end(),
                        [](const TestResult& r) { return r.passed; });
}

int TestSuite::getFailedTests(const std::vector<TestResult>& results) const {
    return std::count_if(results.begin(), results.end(),
                        [](const TestResult& r) { return !r.passed; });
}

double TestSuite::getTotalTime(const std::vector<TestResult>& results) const {
    double total = 0.0;
    for (const auto& result : results) {
        total += result.executionTime;
    }
    return total;
}

double PerformanceTester::measureExecutionTime(std::function<void()> func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0; 
}

void PerformanceTester::benchmarkFunction(const std::string& name, std::function<void()> func, int iterations) {
    std::vector<double> times;
    times.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        times.push_back(measureExecutionTime(func));
    }

    double totalTime = std::accumulate(times.begin(), times.end(), 0.0);
    double avgTime = totalTime / iterations;
    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());

    double variance = 0.0;
    for (double time : times) {
        variance += (time - avgTime) * (time - avgTime);
    }
    double stdDev = std::sqrt(variance / iterations);
    
    std::cout << "\n=== Benchmark: " << name << " ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Average: " << std::fixed << std::setprecision(3) << avgTime << "ms" << std::endl;
    std::cout << "Min: " << minTime << "ms, Max: " << maxTime << "ms" << std::endl;
    std::cout << "Std Dev: " << stdDev << "ms" << std::endl;
    std::cout << "Total: " << totalTime << "ms" << std::endl;
}

void PerformanceTester::memoryUsageTest(const std::string& name, std::function<void()> func) {

    std::cout << "\n=== Memory Test: " << name << " ===" << std::endl;

    std::cout << "Memory usage test not fully implemented" << std::endl;

    func();

    std::cout << "Function executed" << std::endl;
}

std::vector<Quote> MockDataGenerator::generateQuotes(int numQuotes, double spot) {
    std::vector<Quote> quotes;
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<double> expiryDist(0.1, 2.0);

    std::uniform_real_distribution<double> strikeDist(spot * 0.8, spot * 1.2);

    std::uniform_real_distribution<double> volDist(0.10, 0.40);
    
    for (int i = 0; i < numQuotes; ++i) {
        Quote quote;
        quote.strike = strikeDist(gen);
        quote.expiry = expiryDist(gen);
        quote.iv = volDist(gen);
        quote.bid = quote.iv * 0.95; 
        quote.ask = quote.iv * 1.05; 
        quote.volume = std::uniform_int_distribution<int>(100, 10000)(gen);
        quotes.push_back(quote);
    }
    
    return quotes;
}

std::vector<Quote> MockDataGenerator::generateArbitrageFreeQuotes(int numQuotes, double spot) {
    
    auto quotes = generateQuotes(numQuotes, spot);

    std::sort(quotes.begin(), quotes.end(), 
              [](const Quote& a, const Quote& b) {
                  if (std::abs(a.expiry - b.expiry) > 1e-6) {
                      return a.expiry < b.expiry;
                  }
                  return a.strike < b.strike;
              });

    for (auto& quote : quotes) {
        double moneyness = std::log(quote.strike / spot);
        double time = quote.expiry;

        double baseVol = 0.20;
        double skew = -0.1 * moneyness;
        double term = 0.05 * std::sqrt(time);
        
        quote.iv = std::max(0.01, baseVol + skew + term);
        quote.bid = quote.iv * 0.95;
        quote.ask = quote.iv * 1.05;
    }
    
    return quotes;
}

std::vector<Quote> MockDataGenerator::generateButterflyArbitrageQuotes(int numQuotes, double spot) {
    auto quotes = generateArbitrageFreeQuotes(numQuotes, spot);

    for (auto& quote : quotes) {
        if (std::abs(quote.strike - spot) < spot * 0.05) { 
            quote.iv *= 0.5; 
        }
    }
    
    return quotes;
}

std::vector<Quote> MockDataGenerator::generateCalendarArbitrageQuotes(int numQuotes, double spot) {
    auto quotes = generateArbitrageFreeQuotes(numQuotes, spot);

    for (auto& quote : quotes) {
        if (quote.expiry > 1.0) { 
            quote.iv *= 0.7; 
        }
    }
    
    return quotes;
}

MarketData MockDataGenerator::generateMarketData(double spot) {
    MarketData marketData;
    marketData.spot = spot;
    marketData.riskFreeRate = 0.05; 
    marketData.dividendYield = 0.02; 
    marketData.valuationDate = "2024-01-01";
    marketData.currency = "USD";
    return marketData;
}

TestRunner& TestRunner::getInstance() {
    static TestRunner instance;
    return instance;
}

void TestRunner::addSuite(std::unique_ptr<TestSuite> suite) {
    testSuites_.push_back(std::move(suite));
}

int TestRunner::runAllSuites() {
    int totalTests = 0;
    int totalPassed = 0;
    double totalTime = 0.0;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     Running All Test Suites" << std::endl;
    std::cout << "========================================" << std::endl;
    
    for (auto& suite : testSuites_) {
        auto results = suite->runAllTests();
        suite->printResults(results);
        
        totalTests += static_cast<int>(results.size());
        totalPassed += suite->getPassedTests(results);
        totalTime += suite->getTotalTime(results);
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     Overall Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total Tests: " << totalTests << std::endl;
    std::cout << "Passed: " << totalPassed << std::endl;
    std::cout << "Failed: " << (totalTests - totalPassed) << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
              << (100.0 * totalPassed / totalTests) << "%" << std::endl;
    std::cout << "Total Time: " << std::setprecision(3) << totalTime << "s" << std::endl;
    
    return totalTests - totalPassed; 
}

void TestRunner::runSuite(const std::string& suiteName) {
    for (auto& suite : testSuites_) {
        
        auto results = suite->runAllTests();
        suite->printResults(results);
    }
}
