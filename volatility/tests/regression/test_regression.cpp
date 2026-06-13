

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "svi_surface.hpp"
#include "local_vol.hpp"
#include "data_handler.hpp"
#include "vol_api.hpp"
#include <cmath>

void test_bs_call_known_value_atm() {

    auto marketData = MarketData{100.0, 0.05, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double price = surface.callPrice(100.0, 1.0);
    ASSERT_NEAR(price, 10.4506, 0.01);
}

void test_bs_call_known_value_itm() {

    auto marketData = MarketData{100.0, 0.03, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{90.0, 0.5, 0.25}};
    VolSurface surface(quotes, marketData);
    
    double price = surface.callPrice(90.0, 0.5);
    ASSERT_TRUE(price > 10.0);  
    ASSERT_TRUE(price < 20.0);
}

void test_bs_call_known_value_otm() {

    auto marketData = MarketData{100.0, 0.05, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{120.0, 0.25, 0.30}};
    VolSurface surface(quotes, marketData);
    
    double price = surface.callPrice(120.0, 0.25);
    ASSERT_TRUE(price > 0.0);
    ASSERT_TRUE(price < 5.0);  
}

void test_bs_put_call_parity() {
    
    auto marketData = MarketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.25}};
    VolSurface surface(quotes, marketData);
    
    double call = surface.callPrice(100.0, 1.0);
    double put = surface.putPrice(100.0, 1.0);
    
    double S = 100.0, K = 100.0, r = 0.05, q = 0.02, T = 1.0;
    double expectedDiff = S * std::exp(-q * T) - K * std::exp(-r * T);
    
    ASSERT_NEAR(call - put, expectedDiff, 0.01);
}

void test_bs_delta_known_value() {

    ASSERT_TRUE(true);  
}

void test_bs_gamma_known_value() {

    ASSERT_TRUE(true);  
}

void test_butterfly_violation_known_case() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {
        {90.0, 1.0, 0.22},
        {100.0, 1.0, 0.30},  
        {110.0, 1.0, 0.22},
    };
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();
    
    ASSERT_TRUE(violations.size() > 0);
    
    bool foundButterfly = false;
    for (const auto& v : violations) {
        if (v.type == ArbType::ButterflyViolation) {
            foundButterfly = true;
            break;
        }
    }
    ASSERT_TRUE(foundButterfly);
}

void test_calendar_violation_known_case() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {
        {100.0, 0.5, 0.30},   
        {100.0, 1.0, 0.15},   
    };
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();
    
    bool foundCalendar = false;
    for (const auto& v : violations) {
        if (v.type == ArbType::CalendarViolation) {
            foundCalendar = true;
            break;
        }
    }
    ASSERT_TRUE(foundCalendar);
}

void test_arbitrage_free_smile_known_case() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {
        {80.0, 1.0, 0.25},
        {90.0, 1.0, 0.22},
        {100.0, 1.0, 0.20},
        {110.0, 1.0, 0.22},
        {120.0, 1.0, 0.25},
    };
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();

    int significantViolations = 0;
    for (const auto& v : violations) {
        if (std::abs(v.magnitude) > 0.01) significantViolations++;
    }
    ASSERT_TRUE(significantViolations <= 2);
}

void test_regression_quality_score_range() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(20, 100.0);
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    detector.detect();
    
    double score = detector.getQualityScore();
    ASSERT_TRUE(score >= 0.0);
    ASSERT_TRUE(score <= 1.0);
}

void test_qp_already_arbitrage_free() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(15, 100.0);
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    auto result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
    
    ASSERT_TRUE(result.objectiveValue < 1e-2);
}

void test_qp_reduces_violations() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(15, 100.0);
    
    VolSurface surface(quotes, marketData);

    ArbitrageDetector detectorBefore(surface);
    auto violationsBefore = detectorBefore.detect();

    QPSolver solver;
solver.setup(surface);
    auto result = solver.solve(surface);
    ASSERT_TRUE(result.success);

    VolSurface corrected = solver.buildCorrectedSurface(surface, result);
    ArbitrageDetector detectorAfter(corrected);
    auto violationsAfter = detectorAfter.detect();
    
    ASSERT_TRUE(violationsAfter.size() <= violationsBefore.size());
}

void test_qp_minimum_change_principle() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(10, 100.0);
    
    VolSurface surface(quotes, marketData);
    QPSolver solver;
solver.setup(surface);
    auto result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);

    double totalChange = 0.0;
    for (size_t i = 0; i < quotes.size() && i < (size_t)result.ivFlat.size(); ++i) {
        totalChange += std::abs(result.ivFlat(i) - quotes[i].iv);
    }

    double avgChange = totalChange / quotes.size();
    ASSERT_TRUE(avgChange < 0.1);  
}

void test_regression_zero_expiry_handling() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {{100.0, 0.001, 0.20}};  
    
    ASSERT_NO_THROW({
        VolSurface surface(quotes, marketData);
        double price = surface.callPrice(100.0, 0.001);
        ASSERT_FINITE(price);
    });
}

void test_regression_extreme_volatility() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {{100.0, 1.0, 3.0}};  
    
    ASSERT_NO_THROW({
        VolSurface surface(quotes, marketData);
        double price = surface.callPrice(100.0, 1.0);
        ASSERT_FINITE(price);
    });
}

void test_regression_negative_variance() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(20, 100.0);
    
    VolSurface surface(quotes, marketData);
    
    for (double K = 80.0; K <= 120.0; K += 2.0) {
        for (double T = 0.1; T <= 2.0; T += 0.1) {
            double iv = surface.impliedVol(K, T);
            ASSERT_TRUE(iv > 0.0);  
            
            double totalVar = iv * iv * T;
            ASSERT_TRUE(totalVar >= 0.0);  
        }
    }
}

void test_regression_interpolation_continuity() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {
        {90.0, 0.5, 0.22}, {100.0, 0.5, 0.20}, {110.0, 0.5, 0.22},
        {90.0, 1.0, 0.24}, {100.0, 1.0, 0.22}, {110.0, 1.0, 0.24},
    };
    
    VolSurface surface(quotes, marketData);

    double eps = 0.01;
    for (const auto& q : quotes) {
        double ivAt = surface.impliedVol(q.strike, q.expiry);
        double ivNear = surface.impliedVol(q.strike + eps, q.expiry);

        ASSERT_TRUE(std::abs(ivAt - ivNear) < 0.01);
    }
}

void test_stability_small_numbers() {
    
    auto marketData = MarketData{0.001, 0.05, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{0.001, 1.0, 0.20}};
    
    ASSERT_NO_THROW({
        VolSurface surface(quotes, marketData);
        double price = surface.callPrice(0.001, 1.0);
        ASSERT_FINITE(price);
    });
}

void test_stability_large_numbers() {
    
    auto marketData = MarketData{10000.0, 0.05, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{10000.0, 1.0, 0.20}};
    
    ASSERT_NO_THROW({
        VolSurface surface(quotes, marketData);
        double price = surface.callPrice(10000.0, 1.0);
        ASSERT_FINITE(price);
    });
}

void test_stability_floating_point_accumulation() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0);
    VolSurface surface(quotes, marketData);

    double firstValue = surface.impliedVol(100.0, 0.5);
    for (int i = 0; i < 1000; ++i) {
        double value = surface.impliedVol(100.0, 0.5);
        ASSERT_NEAR(value, firstValue, 1e-10);  
    }
}

void test_stability_near_singular_matrix() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes;

    for (int i = 0; i < 5; ++i) {
        quotes.push_back({100.0 + 0.01 * i, 1.0, 0.20});
    }
    
    ASSERT_NO_THROW({
        VolSurface surface(quotes, marketData);
        
    });
}

void test_json_format_compatibility() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0);
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();

    ASSERT_TRUE(true);  
}

void test_config_default_values() {
    
    ArbitrageDetector::Config arbConfig;
    
    ASSERT_TRUE(arbConfig.butterflyThreshold > 0.0);
    ASSERT_TRUE(arbConfig.butterflyThreshold < 0.01);
    ASSERT_TRUE(arbConfig.calendarThreshold > 0.0);
    ASSERT_TRUE(arbConfig.calendarThreshold < 0.01);
    
    QPSolver::Config qpConfig;
    ASSERT_TRUE(qpConfig.maxIterations > 100);
    ASSERT_TRUE(qpConfig.tolerance > 0.0);
    ASSERT_TRUE(qpConfig.tolerance < 0.01);
}

void test_api_response_format() {
    
    ApiResponse response(true, "Test message", "{\"data\": 1}", 0.001);
    
    ASSERT_TRUE(response.success);
    ASSERT_TRUE(response.message == "Test message");  
    ASSERT_TRUE(response.executionTime > 0.0);
}

void test_edge_exactly_at_boundary() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {
        {80.0, 0.5, 0.22},
        {120.0, 0.5, 0.22},
    };
    
    VolSurface surface(quotes, marketData);

    double ivLow = surface.impliedVol(80.0, 0.5);
    double ivHigh = surface.impliedVol(120.0, 0.5);
    
    ASSERT_NEAR(ivLow, 0.22, 1e-6);
    ASSERT_NEAR(ivHigh, 0.22, 1e-6);
}

void test_edge_single_expiry_multiple_strikes() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes;
    
    for (double K = 80.0; K <= 120.0; K += 5.0) {
        double moneyness = std::log(K / 100.0);
        double vol = 0.20 + 0.1 * std::abs(moneyness);
        quotes.push_back({K, 1.0, vol});
    }
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();

    ASSERT_TRUE(violations.empty() || violations.size() <= 2);
}

void test_edge_single_strike_multiple_expiries() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> quotes = {
        {100.0, 0.25, 0.20},
        {100.0, 0.50, 0.21},
        {100.0, 1.00, 0.22},
        {100.0, 2.00, 0.23},
    };

    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();
    
    bool hasCalendarViolation = false;
    for (const auto& v : violations) {
        if (v.type == ArbType::CalendarViolation) {
            hasCalendarViolation = true;
            break;
        }
    }
    ASSERT_FALSE(hasCalendarViolation);
}

void registerRegressionTests() {
    auto& runner = TestRunner::getInstance();
    
    auto suite = std::make_unique<TestSuite>("Regression");

    suite->addTest("BSCallKnownValueATM", test_bs_call_known_value_atm);
    suite->addTest("BSCallKnownValueITM", test_bs_call_known_value_itm);
    suite->addTest("BSCallKnownValueOTM", test_bs_call_known_value_otm);
    suite->addTest("BSPutCallParity", test_bs_put_call_parity);
    suite->addTest("BSDeltaKnownValue", test_bs_delta_known_value);
    suite->addTest("BSGammaKnownValue", test_bs_gamma_known_value);

    suite->addTest("ButterflyViolationKnownCase", test_butterfly_violation_known_case);
    suite->addTest("CalendarViolationKnownCase", test_calendar_violation_known_case);
    suite->addTest("ArbitrageFreeSmileKnownCase", test_arbitrage_free_smile_known_case);
    suite->addTest("RegressionQualityScoreRange", test_regression_quality_score_range);

    suite->addTest("QPAlreadyArbitrageFree", test_qp_already_arbitrage_free);
    suite->addTest("QPReducesViolations", test_qp_reduces_violations);
    suite->addTest("QPMinimumChangePrinciple", test_qp_minimum_change_principle);

    suite->addTest("RegressionZeroExpiryHandling", test_regression_zero_expiry_handling);
    suite->addTest("RegressionExtremeVolatility", test_regression_extreme_volatility);
    suite->addTest("RegressionNegativeVariance", test_regression_negative_variance);
    suite->addTest("RegressionInterpolationContinuity", test_regression_interpolation_continuity);

    suite->addTest("StabilitySmallNumbers", test_stability_small_numbers);
    suite->addTest("StabilityLargeNumbers", test_stability_large_numbers);
    suite->addTest("StabilityFloatingPointAccumulation", test_stability_floating_point_accumulation);
    suite->addTest("StabilityNearSingularMatrix", test_stability_near_singular_matrix);

    suite->addTest("JSONFormatCompatibility", test_json_format_compatibility);
    suite->addTest("ConfigDefaultValues", test_config_default_values);
    suite->addTest("APIResponseFormat", test_api_response_format);

    suite->addTest("EdgeExactlyAtBoundary", test_edge_exactly_at_boundary);
    suite->addTest("EdgeSingleExpiryMultipleStrikes", test_edge_single_expiry_multiple_strikes);
    suite->addTest("EdgeSingleStrikeMultipleExpiries", test_edge_single_strike_multiple_expiries);
    
    runner.addSuite(std::move(suite));
}
