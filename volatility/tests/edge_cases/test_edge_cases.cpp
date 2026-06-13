

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "data_handler.hpp"
#include <cmath>
#include <limits>

void test_edge_very_low_vol() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.001}};
    
    ASSERT_NO_THROW(VolSurface surface(quotes, marketData));
}

void test_edge_very_high_vol() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 5.0}};  
    
    ASSERT_NO_THROW(VolSurface surface(quotes, marketData));
}

void test_edge_zero_vol() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.0}};

    bool threw = false;
    try {
        VolSurface surface(quotes, marketData);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

void test_edge_near_zero_expiry() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 0.0001, 0.20}};  
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 0.0001);
    ASSERT_FINITE(callPrice);
}

void test_edge_very_long_expiry() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 30.0, 0.20}};  
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 30.0);
    ASSERT_FINITE(callPrice);
    ASSERT_TRUE(callPrice > 0.0);
}

void test_edge_one_day_expiry() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    double oneDayYears = 1.0 / 365.0;
    std::vector<Quote> quotes = {{100.0, oneDayYears, 0.20}};
    
    VolSurface surface(quotes, marketData);
    ASSERT_NO_THROW(surface.callPrice(100.0, oneDayYears));
}

void test_edge_very_low_strike() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{1.0, 1.0, 0.50}};  
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(1.0, 1.0);
    ASSERT_FINITE(callPrice);
    ASSERT_TRUE(callPrice > 95.0);  
}

void test_edge_very_high_strike() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{1000.0, 1.0, 0.50}};  
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(1000.0, 1.0);
    ASSERT_FINITE(callPrice);
    ASSERT_TRUE(callPrice >= 0.0);
    ASSERT_TRUE(callPrice < 1.0);  
}

void test_edge_strike_equals_spot() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 1.0);
    double putPrice = surface.putPrice(100.0, 1.0);

    ASSERT_FINITE(callPrice);
    ASSERT_FINITE(putPrice);
}

void test_edge_negative_rate() {
    MarketData marketData{100.0, -0.05, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 1.0);
    ASSERT_FINITE(callPrice);
    ASSERT_TRUE(callPrice > 0.0);
}

void test_edge_high_rate() {
    MarketData marketData{100.0, 0.50, 0.0, "2024-01-01", "USD"};  
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 1.0);
    ASSERT_FINITE(callPrice);
}

void test_edge_zero_rate() {
    MarketData marketData{100.0, 0.0, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 1.0);
    ASSERT_FINITE(callPrice);
}

void test_edge_high_dividend() {
    MarketData marketData{100.0, 0.05, 0.20, "2024-01-01", "USD"};  
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 1.0);
    ASSERT_FINITE(callPrice);
}

void test_edge_dividend_exceeds_rate() {
    MarketData marketData{100.0, 0.02, 0.08, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double callPrice = surface.callPrice(100.0, 1.0);
    ASSERT_FINITE(callPrice);
}

void test_edge_very_close_strikes() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {
        {100.0, 1.0, 0.20},
        {100.0001, 1.0, 0.2001}
    };
    
    VolSurface surface(quotes, marketData);
    double iv1 = surface.impliedVol(100.0, 1.0);
    double iv2 = surface.impliedVol(100.0001, 1.0);
    
    ASSERT_FINITE(iv1);
    ASSERT_FINITE(iv2);
    ASSERT_NEAR(iv1, iv2, 0.01);
}

void test_edge_very_close_expiries() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {
        {100.0, 1.0, 0.20},
        {100.0, 1.0001, 0.2001}
    };
    
    VolSurface surface(quotes, marketData);
    double iv1 = surface.impliedVol(100.0, 1.0);
    double iv2 = surface.impliedVol(100.0, 1.0001);
    
    ASSERT_FINITE(iv1);
    ASSERT_FINITE(iv2);
}

void test_edge_interpolation_at_boundary() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {
        {80.0, 0.25, 0.22},
        {100.0, 0.25, 0.20},
        {120.0, 0.25, 0.23},
        {80.0, 1.0, 0.24},
        {100.0, 1.0, 0.22},
        {120.0, 1.0, 0.25}
    };
    
    VolSurface surface(quotes, marketData);

    double ivGridPoint = surface.impliedVol(100.0, 0.25);
    ASSERT_NEAR(ivGridPoint, 0.20, 0.01);

    double ivJustInside = surface.impliedVol(80.01, 0.26);
    ASSERT_FINITE(ivJustInside);
}

void test_edge_single_quote() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double iv = surface.impliedVol(100.0, 1.0);
    
    ASSERT_NEAR(iv, 0.20, 0.001);
}

void test_edge_two_quotes_same_expiry() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {
        {90.0, 1.0, 0.22},
        {110.0, 1.0, 0.22}
    };
    
    VolSurface surface(quotes, marketData);
    double ivInterp = surface.impliedVol(100.0, 1.0);
    
    ASSERT_FINITE(ivInterp);
}

void test_edge_two_quotes_same_strike() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {
        {100.0, 0.5, 0.18},
        {100.0, 1.0, 0.20}
    };
    
    VolSurface surface(quotes, marketData);
    double ivInterp = surface.impliedVol(100.0, 0.75);
    
    ASSERT_FINITE(ivInterp);
    ASSERT_IN_RANGE(ivInterp, 0.18, 0.21);
}

void test_edge_large_surface() {
    auto quotes = ExtendedMockDataGenerator::generateLargeDataset(50, 20, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    ASSERT_SIZE(quotes, 1000);
    
    VolSurface surface(quotes, marketData);
    double iv = surface.impliedVol(100.0, 1.0);
    
    ASSERT_FINITE(iv);
}

void test_edge_arb_flat_surface() {
    std::vector<Quote> quotes;
    for (double T : {0.25, 0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            quotes.push_back({K, T, 0.20});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();

    int significantViolations = 0;
    for (const auto& v : violations) {
        if (std::abs(v.magnitude) > 0.01) significantViolations++;
    }
    
    ASSERT_TRUE(significantViolations <= 5);
}

void test_edge_arb_steep_smile() {
    std::vector<Quote> quotes;
    for (double T : {0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            double logM = std::log(K / 100.0);
            double iv = 0.20 + 0.50 * logM * logM;  
            quotes.push_back({K, T, iv});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    ASSERT_NO_THROW(detector.detect());
}

void test_edge_arb_skewed_surface() {
    std::vector<Quote> quotes;
    for (double T : {0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            double logM = std::log(K / 100.0);
            double iv = 0.25 - 0.20 * logM;  
            quotes.push_back({K, T, iv});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    ASSERT_NO_THROW(detector.detect());
}

void test_edge_qp_minimal_surface() {
    std::vector<Quote> quotes = {
        {90.0, 0.5, 0.22},
        {100.0, 0.5, 0.20},
        {110.0, 0.5, 0.22}
    };
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    QPSolver solver;
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_edge_qp_severe_violation() {
    std::vector<Quote> quotes;
    for (double T : {0.25, 0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            double logM = std::log(K / 100.0);
            double baseVol = 0.20 + 0.05 * std::abs(logM);
            quotes.push_back({K, T, baseVol});
        }
    }

    for (auto& q : quotes) {
        if (std::abs(q.strike - 100.0) < 1.0 && std::abs(q.expiry - 0.5) < 0.01) {
            q.iv = 0.01;
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    QPSolver solver;
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_edge_qp_all_same_vol() {
    std::vector<Quote> quotes;
    for (double T : {0.25, 0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            quotes.push_back({K, T, 0.20});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    
    QPSolver solver;
solver.setup(surface);
    QPResult result = solver.solve(surface);
    
    ASSERT_TRUE(result.success);
}

void test_edge_subnormal_values() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);

    double result = surface.impliedVol(100.0 + 1e-300, 1.0);
    ASSERT_FINITE(result);
}

void test_edge_maximum_double() {

    MarketData marketData{1e6, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{1e6, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    double result = surface.callPrice(1e6, 1.0);
    ASSERT_FINITE(result);
}

void test_edge_minimum_positive() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1e-10, 0.20}};
    
    VolSurface surface(quotes, marketData);
    
    ASSERT_NO_THROW(surface.impliedVol(100.0, 1e-10));
}

void test_edge_deterministic_results() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    
    double result1 = surface.callPrice(100.0, 1.0);
    double result2 = surface.callPrice(100.0, 1.0);
    double result3 = surface.callPrice(100.0, 1.0);
    
    ASSERT_EQ(result1, result2);
    ASSERT_EQ(result2, result3);
}

void test_edge_small_perturbation_stability() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surface(quotes, marketData);
    
    double base = surface.callPrice(100.0, 1.0);
    double perturbed = surface.callPrice(100.0 + 1e-10, 1.0);

    ASSERT_NEAR(base, perturbed, 1e-6);
}

std::unique_ptr<TestSuite> createEdgeCaseTestSuite() {
    auto suite = std::make_unique<TestSuite>("Edge Case Tests");

    suite->addTest("Edge Very Low Vol", test_edge_very_low_vol);
    suite->addTest("Edge Very High Vol", test_edge_very_high_vol);
    suite->addTest("Edge Zero Vol", test_edge_zero_vol);

    suite->addTest("Edge Near Zero Expiry", test_edge_near_zero_expiry);
    suite->addTest("Edge Very Long Expiry", test_edge_very_long_expiry);
    suite->addTest("Edge One Day Expiry", test_edge_one_day_expiry);

    suite->addTest("Edge Very Low Strike", test_edge_very_low_strike);
    suite->addTest("Edge Very High Strike", test_edge_very_high_strike);
    suite->addTest("Edge Strike Equals Spot", test_edge_strike_equals_spot);

    suite->addTest("Edge Negative Rate", test_edge_negative_rate);
    suite->addTest("Edge High Rate", test_edge_high_rate);
    suite->addTest("Edge Zero Rate", test_edge_zero_rate);

    suite->addTest("Edge High Dividend", test_edge_high_dividend);
    suite->addTest("Edge Dividend Exceeds Rate", test_edge_dividend_exceeds_rate);

    suite->addTest("Edge Very Close Strikes", test_edge_very_close_strikes);
    suite->addTest("Edge Very Close Expiries", test_edge_very_close_expiries);
    suite->addTest("Edge Interpolation At Boundary", test_edge_interpolation_at_boundary);

    suite->addTest("Edge Single Quote", test_edge_single_quote);
    suite->addTest("Edge Two Quotes Same Expiry", test_edge_two_quotes_same_expiry);
    suite->addTest("Edge Two Quotes Same Strike", test_edge_two_quotes_same_strike);
    suite->addTest("Edge Large Surface", test_edge_large_surface);

    suite->addTest("Edge Arb Flat Surface", test_edge_arb_flat_surface);
    suite->addTest("Edge Arb Steep Smile", test_edge_arb_steep_smile);
    suite->addTest("Edge Arb Skewed Surface", test_edge_arb_skewed_surface);

    suite->addTest("Edge QP Minimal Surface", test_edge_qp_minimal_surface);
    suite->addTest("Edge QP Severe Violation", test_edge_qp_severe_violation);
    suite->addTest("Edge QP All Same Vol", test_edge_qp_all_same_vol);

    suite->addTest("Edge Subnormal Values", test_edge_subnormal_values);
    suite->addTest("Edge Maximum Double", test_edge_maximum_double);
    suite->addTest("Edge Minimum Positive", test_edge_minimum_positive);

    suite->addTest("Edge Deterministic Results", test_edge_deterministic_results);
    suite->addTest("Edge Small Perturbation Stability", test_edge_small_perturbation_stability);
    
    return suite;
}

void registerEdgeCaseTests() {
    auto& runner = TestRunner::getInstance();
    runner.addSuite(createEdgeCaseTestSuite());
}
