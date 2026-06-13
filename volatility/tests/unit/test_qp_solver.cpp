

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include <cmath>

void test_qp_solver_construction() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    ASSERT_NO_THROW({ QPSolver solver; solver.setup(surface); });
}

void test_qp_solver_with_config() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    QPSolver::Config config;
    config.maxIterations = 500;
    config.tolerance = 1e-8;
    config.regularizationWeight = 0.01;
    
    ASSERT_NO_THROW({ QPSolver solver(config); solver.setup(surface); });
}

void test_qp_solver_solve_arbitrage_free() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_qp_solver_repairs_butterfly() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);

    ArbitrageDetector detectorBefore(surface);
    auto violationsBefore = detectorBefore.detect();
    ASSERT_NOT_EMPTY(violationsBefore);

    QPSolver solver;
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);

    VolSurface corrected = solver.buildCorrectedSurface(surface, result);

    ArbitrageDetector detectorAfter(corrected);
    auto violationsAfter = detectorAfter.detect();
    
    ASSERT_TRUE(violationsAfter.size() <= violationsBefore.size());
}

void test_qp_solver_repairs_calendar() {
    auto quotes = MockDataGenerator::generateCalendarArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);

    ArbitrageDetector detectorBefore(surface);
    auto violationsBefore = detectorBefore.detect();
    ASSERT_NOT_EMPTY(violationsBefore);

    QPSolver solver;
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_qp_solver_minimizes_perturbation() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);

    double maxPerturbation = 0.0;
    for (int i = 0; i < result.ivFlat.size(); ++i) {
        maxPerturbation = std::max(maxPerturbation, std::abs(result.ivFlat(i)));
    }

    ASSERT_TRUE(maxPerturbation < 1.0);
}

void test_qp_result_iterations() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.iterations > 0);
    ASSERT_TRUE(result.iterations <= 2000);  
}

void test_qp_result_objective_value() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);

    ASSERT_FINITE(result.objectiveValue);
}

void test_qp_result_solve_time() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);

    ASSERT_TRUE(result.solveTime >= 0.0);
    ASSERT_TRUE(result.solveTime < 10.0);
}

void test_qp_result_status_message() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_FALSE(result.status.empty());
}

void test_qp_config_regularization_weight() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);

    QPSolver::Config configHigh;
    configHigh.regularizationWeight = 1.0;
    QPSolver solverHigh(configHigh);
solverHigh.setup(surface);
    QPResult resultHigh = solverHigh.solve(surface);

    QPSolver::Config configLow;
    configLow.regularizationWeight = 0.001;
    QPSolver solverLow(configLow);
solverLow.setup(surface);
    QPResult resultLow = solverLow.solve(surface);

    ASSERT_TRUE(resultHigh.success);
    ASSERT_TRUE(resultLow.success);
}

void test_qp_config_smoothness_weight() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    QPSolver::Config config;
    config.smoothnessWeight = 0.5;
    
    QPSolver solver(config);
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_qp_config_max_iterations() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    QPSolver::Config config;
    config.maxIterations = 100;
    
    QPSolver solver(config);
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.iterations <= 100);
}

void test_qp_config_tolerance() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);

    QPSolver::Config configTight;
    configTight.tolerance = 1e-10;
    QPSolver solverTight(configTight);
solverTight.setup(surface);
    QPResult resultTight = solverTight.solve(surface);

    QPSolver::Config configLoose;
    configLoose.tolerance = 1e-4;
    QPSolver solverLoose(configLoose);
solverLoose.setup(surface);
    QPResult resultLoose = solverLoose.solve(surface);

    ASSERT_TRUE(resultTight.success);
    ASSERT_TRUE(resultLoose.success);
}

void test_qp_config_vol_bounds() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    QPSolver::Config config;
    config.minVol = 0.01;
    config.maxVol = 2.0;
    
    QPSolver solver(config);
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);

    for (int i = 0; i < result.ivFlat.size(); ++i) {
        ASSERT_TRUE(result.ivFlat(i) >= config.minVol - 0.001);
        ASSERT_TRUE(result.ivFlat(i) <= config.maxVol + 0.001);
    }
}

void test_qp_small_surface() {
    std::vector<Quote> quotes = {
        {90.0, 0.5, 0.20},
        {100.0, 0.5, 0.18},
        {110.0, 0.5, 0.22}
    };
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_qp_large_surface() {
    auto quotes = ExtendedMockDataGenerator::generateLargeDataset(20, 10, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_qp_high_vol_surface() {
    std::vector<Quote> quotes;
    for (double T : {0.25, 0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            double logM = std::log(K / 100.0);
            double iv = 1.5 + 0.3 * logM * logM;  
            quotes.push_back({K, T, iv});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_qp_low_vol_surface() {
    std::vector<Quote> quotes;
    for (double T : {0.25, 0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            double logM = std::log(K / 100.0);
            double iv = 0.05 + 0.01 * logM * logM;  
            quotes.push_back({K, T, iv});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_build_corrected_surface() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    ASSERT_TRUE(result.success);
    
    VolSurface corrected = solver.buildCorrectedSurface(surface, result);

    double testVol = corrected.impliedVol(100.0, 0.5);
    ASSERT_TRUE(testVol > 0);
    ASSERT_FINITE(testVol);
}

void test_corrected_surface_maintains_structure() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    
    QPResult result = solver.solve(surface);
    VolSurface corrected = solver.buildCorrectedSurface(surface, result);

    ASSERT_EQ(surface.strikes().size(), corrected.strikes().size());
    ASSERT_EQ(surface.expiries().size(), corrected.expiries().size());
}

std::unique_ptr<TestSuite> createQPSolverTestSuite() {
    auto suite = std::make_unique<TestSuite>("QP Solver Unit Tests");

    suite->addTest("QP Solver Construction", test_qp_solver_construction);
    suite->addTest("QP Solver With Config", test_qp_solver_with_config);
    suite->addTest("QP Solver Solve Arbitrage-Free", test_qp_solver_solve_arbitrage_free);

    suite->addTest("QP Solver Repairs Butterfly", test_qp_solver_repairs_butterfly);
    suite->addTest("QP Solver Repairs Calendar", test_qp_solver_repairs_calendar);
    suite->addTest("QP Solver Minimizes Perturbation", test_qp_solver_minimizes_perturbation);

    suite->addTest("QP Result Iterations", test_qp_result_iterations);
    suite->addTest("QP Result Objective Value", test_qp_result_objective_value);
    suite->addTest("QP Result Solve Time", test_qp_result_solve_time);
    suite->addTest("QP Result Status Message", test_qp_result_status_message);

    suite->addTest("QP Config Regularization Weight", test_qp_config_regularization_weight);
    suite->addTest("QP Config Smoothness Weight", test_qp_config_smoothness_weight);
    suite->addTest("QP Config Max Iterations", test_qp_config_max_iterations);
    suite->addTest("QP Config Tolerance", test_qp_config_tolerance);
    suite->addTest("QP Config Vol Bounds", test_qp_config_vol_bounds);

    suite->addTest("QP Small Surface", test_qp_small_surface);
    suite->addTest("QP Large Surface", test_qp_large_surface);
    suite->addTest("QP High Vol Surface", test_qp_high_vol_surface);
    suite->addTest("QP Low Vol Surface", test_qp_low_vol_surface);

    suite->addTest("Build Corrected Surface", test_build_corrected_surface);
    suite->addTest("Corrected Surface Maintains Structure", test_corrected_surface_maintains_structure);
    
    return suite;
}

void registerQPSolverTests() {
    auto& runner = TestRunner::getInstance();
    runner.addSuite(createQPSolverTestSuite());
}
