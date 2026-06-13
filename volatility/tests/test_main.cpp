

#include "test_framework.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

extern void registerBlackScholesTests();
extern void registerArbitrageDetectorTests();
extern void registerQPSolverTests();
extern void registerEdgeCaseTests();
extern void registerStressTests();
extern void registerBenchmarkTests();
extern void registerThreadSafetyTests();
extern void registerRegressionTests();
extern void registerIntegrationTests();

struct TestOptions {
    std::vector<std::string> suites;
    bool verbose = false;
    bool showHelp = false;
    bool listSuites = false;
};

TestOptions parseArgs(int argc, char* argv[]) {
    TestOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            opts.showHelp = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--list" || arg == "-l") {
            opts.listSuites = true;
        } else if (arg.find("--suite=") == 0) {
            
            std::string suiteList = arg.substr(8);
            size_t pos = 0;
            while ((pos = suiteList.find(',')) != std::string::npos) {
                opts.suites.push_back(suiteList.substr(0, pos));
                suiteList.erase(0, pos + 1);
            }
            if (!suiteList.empty()) {
                opts.suites.push_back(suiteList);
            }
        }
    }
    
    return opts;
}

void showHelp() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  Volatility Arbitrage Test Suite\n";
    std::cout << "================================================================\n\n";
    std::cout << "Usage: vol_arb_tests [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h        Show this help message\n";
    std::cout << "  --verbose, -v     Enable verbose output\n";
    std::cout << "  --list, -l        List available test suites\n";
    std::cout << "  --suite=<names>   Run specific suite(s), comma-separated\n\n";
    std::cout << "Examples:\n";
    std::cout << "  vol_arb_tests                              Run all tests\n";
    std::cout << "  vol_arb_tests --suite=BlackScholes         Run BlackScholes tests\n";
    std::cout << "  vol_arb_tests --suite=QPSolver,EdgeCases   Run multiple suites\n";
    std::cout << "  vol_arb_tests --verbose                    Run with verbose output\n\n";
}

int main(int argc, char* argv[]) {
    TestOptions opts = parseArgs(argc, argv);
    
    if (opts.showHelp) {
        showHelp();
        return 0;
    }
    
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  Volatility Arbitrage Test Suite v1.0\n";
    std::cout << "================================================================\n\n";

    auto& runner = TestRunner::getInstance();

    std::cout << "Registering test suites...\n";
    
    try {
        registerBlackScholesTests();
        std::cout << "  [OK] BlackScholes tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] BlackScholes registration failed: " << e.what() << "\n";
    }
    
    try {
        registerArbitrageDetectorTests();
        std::cout << "  [OK] ArbitrageDetector tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] ArbitrageDetector registration failed: " << e.what() << "\n";
    }
    
    try {
        registerQPSolverTests();
        std::cout << "  [OK] QPSolver tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] QPSolver registration failed: " << e.what() << "\n";
    }
    
    try {
        registerEdgeCaseTests();
        std::cout << "  [OK] EdgeCase tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] EdgeCase registration failed: " << e.what() << "\n";
    }
    
    try {
        registerStressTests();
        std::cout << "  [OK] Stress tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] Stress registration failed: " << e.what() << "\n";
    }
    
    try {
        registerBenchmarkTests();
        std::cout << "  [OK] Benchmark tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] Benchmark registration failed: " << e.what() << "\n";
    }
    
    try {
        registerThreadSafetyTests();
        std::cout << "  [OK] ThreadSafety tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] ThreadSafety registration failed: " << e.what() << "\n";
    }
    
    try {
        registerRegressionTests();
        std::cout << "  [OK] Regression tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] Regression registration failed: " << e.what() << "\n";
    }
    
    try {
        registerIntegrationTests();
        std::cout << "  [OK] Integration tests registered\n";
    } catch (const std::exception& e) {
        std::cout << "  [WARN] Integration registration failed: " << e.what() << "\n";
    }
    
    std::cout << "\n";

    if (opts.listSuites) {
        std::cout << "Available test suites:\n";
        std::cout << "  - BlackScholes\n";
        std::cout << "  - ArbitrageDetector\n";
        std::cout << "  - QPSolver\n";
        std::cout << "  - EdgeCases\n";
        std::cout << "  - Stress\n";
        std::cout << "  - Performance\n";
        std::cout << "  - ThreadSafety\n";
        std::cout << "  - Regression\n";
        std::cout << "  - Integration\n";
        return 0;
    }

    std::cout << "================================================================\n";
    std::cout << "  Running Tests\n";
    std::cout << "================================================================\n\n";

    int failures = runner.runAllSuites();

    if (failures == 0) {
        std::cout << "\n[SUCCESS] All tests passed!\n\n";
        return 0;
    } else {
        std::cout << "\n[FAILURE] " << failures << " test(s) failed.\n\n";
        return 1;
    }
}
