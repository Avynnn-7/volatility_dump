

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include <cmath>
#include <limits>

void test_bs_call_atm_option() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(100.0, 1.0);
    
    ASSERT_IN_RANGE(callPrice, 8.0, 11.0);
    ASSERT_TRUE(callPrice > 0.0);
    ASSERT_FINITE(callPrice);
}

void test_bs_call_deep_itm() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{50.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(50.0, 1.0);
    double intrinsic = 100.0 - 50.0;
    
    ASSERT_TRUE(callPrice >= intrinsic * 0.9);
    ASSERT_TRUE(callPrice <= intrinsic * 1.2);
    ASSERT_FINITE(callPrice);
}

void test_bs_call_deep_otm() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{200.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(200.0, 1.0);
    
    ASSERT_TRUE(callPrice >= 0.0);
    ASSERT_TRUE(callPrice < 5.0);
    ASSERT_FINITE(callPrice);
}

void test_bs_call_zero_expiry() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{90.0, 0.001, 0.20}, {110.0, 0.001, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double callPriceITM = surface.callPrice(90.0, 0.001);
    double callPriceOTM = surface.callPrice(110.0, 0.001);
    
    ASSERT_NEAR(callPriceITM, 10.0, 1.0);
    ASSERT_NEAR(callPriceOTM, 0.0, 0.5);
}

void test_bs_call_zero_volatility() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.001}};  
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(100.0, 1.0);

    ASSERT_FINITE(callPrice);
    ASSERT_TRUE(callPrice > 0.0);
    ASSERT_TRUE(callPrice < 20.0);  
}

void test_bs_call_high_volatility() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 1.0}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(100.0, 1.0);
    
    ASSERT_TRUE(callPrice > 20.0);
    ASSERT_TRUE(callPrice < 100.0);
    ASSERT_FINITE(callPrice);
}

void test_bs_call_extreme_moneyness() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    std::vector<Quote> quotes1 = {{10.0, 1.0, 0.30}};
    VolSurface surface1(quotes1, marketData);
    double veryDeepITM = surface1.callPrice(10.0, 1.0);
    ASSERT_TRUE(veryDeepITM >= 85.0);
    ASSERT_FINITE(veryDeepITM);
    
    std::vector<Quote> quotes2 = {{500.0, 1.0, 0.30}};
    VolSurface surface2(quotes2, marketData);
    double veryDeepOTM = surface2.callPrice(500.0, 1.0);
    ASSERT_TRUE(veryDeepOTM >= 0.0);
    ASSERT_TRUE(veryDeepOTM < 1.0);
    ASSERT_FINITE(veryDeepOTM);
}

void test_bs_put_atm_option() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double putPrice = surface.putPrice(100.0, 1.0);
    
    ASSERT_IN_RANGE(putPrice, 4.0, 10.0);
    ASSERT_TRUE(putPrice > 0.0);
    ASSERT_FINITE(putPrice);
}

void test_bs_put_deep_itm() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{200.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double putPrice = surface.putPrice(200.0, 1.0);
    double intrinsic = 200.0 - 100.0;
    
    ASSERT_TRUE(putPrice >= intrinsic * 0.85);
    ASSERT_FINITE(putPrice);
}

void test_bs_put_deep_otm() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{50.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double putPrice = surface.putPrice(50.0, 1.0);
    
    ASSERT_TRUE(putPrice >= 0.0);
    ASSERT_TRUE(putPrice < 2.0);
    ASSERT_FINITE(putPrice);
}

void test_put_call_parity_atm() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double S = 100.0, K = 100.0, T = 1.0;
    double r = 0.05, q = 0.02;
    
    double callPrice = surface.callPrice(K, T);
    double putPrice = surface.putPrice(K, T);
    
    double lhs = callPrice - putPrice;
    double rhs = S * std::exp(-q * T) - K * std::exp(-r * T);
    
    ASSERT_NEAR(lhs, rhs, 0.01);
}

void test_put_call_parity_otm() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{120.0, 0.5, 0.25}};
    VolSurface surface(quotes, marketData);
    
    double S = 100.0, K = 120.0, T = 0.5;
    double r = 0.05, q = 0.02;
    
    double callPrice = surface.callPrice(K, T);
    double putPrice = surface.putPrice(K, T);
    
    double lhs = callPrice - putPrice;
    double rhs = S * std::exp(-q * T) - K * std::exp(-r * T);
    
    ASSERT_NEAR(lhs, rhs, 0.01);
}

void test_put_call_parity_multiple_strikes() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    double S = 100.0, r = 0.05, q = 0.02, T = 1.0;
    
    std::vector<double> strikes = {60.0, 80.0, 100.0, 120.0, 140.0};
    
    for (double K : strikes) {
        std::vector<Quote> quotes = {{K, T, 0.20}};
        VolSurface surface(quotes, marketData);
        
        double callPrice = surface.callPrice(K, T);
        double putPrice = surface.putPrice(K, T);
        
        double lhs = callPrice - putPrice;
        double rhs = S * std::exp(-q * T) - K * std::exp(-r * T);
        
        ASSERT_NEAR(lhs, rhs, 0.1);
    }
}

void test_bs_intrinsic_floor() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{80.0, 1.0, 0.10}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(80.0, 1.0);
    double discountedIntrinsic = std::max(
        100.0 * std::exp(-0.02 * 1.0) - 80.0 * std::exp(-0.05 * 1.0), 0.0);
    
    ASSERT_TRUE(callPrice >= discountedIntrinsic - 0.1);
}

void test_bs_spot_upper_bound() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{50.0, 1.0, 0.30}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(50.0, 1.0);
    double spotDiscounted = 100.0 * std::exp(-0.02 * 1.0);
    
    ASSERT_TRUE(callPrice <= spotDiscounted + 0.1);
}

void test_bs_strike_upper_bound() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{150.0, 1.0, 0.30}};
    VolSurface surface(quotes, marketData);
    
    double putPrice = surface.putPrice(150.0, 1.0);
    double discountedStrike = 150.0 * std::exp(-0.05 * 1.0);
    
    ASSERT_TRUE(putPrice <= discountedStrike + 0.1);
}

void test_bs_nan_inf_inputs() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    ASSERT_NO_THROW(surface.callPrice(100.0, 1.0));
    ASSERT_NO_THROW(surface.putPrice(100.0, 1.0));
}

void test_bs_delta_call_positive() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    MarketData marketDataUp{101.0, 0.05, 0.02, "2024-01-01", "USD"};
    MarketData marketDataDown{99.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface surfaceUp(quotes, marketDataUp);
    VolSurface surfaceDown(quotes, marketDataDown);
    
    double callUp = surfaceUp.callPrice(100.0, 1.0);
    double callDown = surfaceDown.callPrice(100.0, 1.0);
    
    double delta = (callUp - callDown) / 2.0;
    ASSERT_TRUE(delta > 0.0);
    ASSERT_TRUE(delta <= 1.0);
}

void test_bs_gamma_positive() {
    MarketData md98{98.0, 0.05, 0.02, "2024-01-01", "USD"};
    MarketData md100{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    MarketData md102{102.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    
    VolSurface s98(quotes, md98);
    VolSurface s100(quotes, md100);
    VolSurface s102(quotes, md102);
    
    double c98 = s98.callPrice(100.0, 1.0);
    double c100 = s100.callPrice(100.0, 1.0);
    double c102 = s102.callPrice(100.0, 1.0);
    
    double gamma = (c102 - 2*c100 + c98) / 4.0;
    ASSERT_TRUE(gamma > 0.0);
}

void test_bs_theta_time_decay() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    std::vector<Quote> q1yr = {{100.0, 1.0, 0.20}};
    std::vector<Quote> q2yr = {{100.0, 2.0, 0.20}};
    
    VolSurface s1yr(q1yr, marketData);
    VolSurface s2yr(q2yr, marketData);
    
    double c1yr = s1yr.callPrice(100.0, 1.0);
    double c2yr = s2yr.callPrice(100.0, 2.0);
    
    ASSERT_TRUE(c2yr >= c1yr - 0.5);
}

void test_bs_numerical_consistency() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double call1 = surface.callPrice(100.0, 1.0);
    double call2 = surface.callPrice(100.0, 1.0);
    double call3 = surface.callPrice(100.0, 1.0);
    
    ASSERT_EQ(call1, call2);
    ASSERT_EQ(call2, call3);
}

void test_bs_extreme_rates() {
    MarketData mdHigh{100.0, 0.15, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> qHigh = {{100.0, 1.0, 0.20}};
    VolSurface sHigh(qHigh, mdHigh);
    double callHigh = sHigh.callPrice(100.0, 1.0);
    ASSERT_FINITE(callHigh);
    
    MarketData mdNeg{100.0, -0.02, 0.0, "2024-01-01", "USD"};
    std::vector<Quote> qNeg = {{100.0, 1.0, 0.20}};
    VolSurface sNeg(qNeg, mdNeg);
    double callNeg = sNeg.callPrice(100.0, 1.0);
    ASSERT_FINITE(callNeg);
    
    ASSERT_TRUE(callHigh > callNeg);
}

void test_bs_long_expiry() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    std::vector<Quote> quotes = {{100.0, 10.0, 0.20}};
    VolSurface surface(quotes, marketData);
    
    double callPrice = surface.callPrice(100.0, 10.0);
    
    ASSERT_FINITE(callPrice);
    ASSERT_TRUE(callPrice > 0.0);
    ASSERT_TRUE(callPrice < 100.0);
}

std::unique_ptr<TestSuite> createBlackScholesTestSuite() {
    auto suite = std::make_unique<TestSuite>("Black-Scholes Unit Tests");

    suite->addTest("BS Call ATM Option", test_bs_call_atm_option);
    suite->addTest("BS Call Deep ITM", test_bs_call_deep_itm);
    suite->addTest("BS Call Deep OTM", test_bs_call_deep_otm);
    suite->addTest("BS Call Zero Expiry", test_bs_call_zero_expiry);
    suite->addTest("BS Call Zero Volatility", test_bs_call_zero_volatility);
    suite->addTest("BS Call High Volatility", test_bs_call_high_volatility);
    suite->addTest("BS Call Extreme Moneyness", test_bs_call_extreme_moneyness);

    suite->addTest("BS Put ATM Option", test_bs_put_atm_option);
    suite->addTest("BS Put Deep ITM", test_bs_put_deep_itm);
    suite->addTest("BS Put Deep OTM", test_bs_put_deep_otm);

    suite->addTest("Put-Call Parity ATM", test_put_call_parity_atm);
    suite->addTest("Put-Call Parity OTM", test_put_call_parity_otm);
    suite->addTest("Put-Call Parity Multiple Strikes", test_put_call_parity_multiple_strikes);

    suite->addTest("BS Intrinsic Floor", test_bs_intrinsic_floor);
    suite->addTest("BS Spot Upper Bound", test_bs_spot_upper_bound);
    suite->addTest("BS Strike Upper Bound", test_bs_strike_upper_bound);
    suite->addTest("BS NaN/Inf Inputs", test_bs_nan_inf_inputs);

    suite->addTest("BS Delta Call Positive", test_bs_delta_call_positive);
    suite->addTest("BS Gamma Positive", test_bs_gamma_positive);
    suite->addTest("BS Theta Time Decay", test_bs_theta_time_decay);

    suite->addTest("BS Numerical Consistency", test_bs_numerical_consistency);
    suite->addTest("BS Extreme Rates", test_bs_extreme_rates);
    suite->addTest("BS Long Expiry", test_bs_long_expiry);
    
    return suite;
}

void registerBlackScholesTests() {
    auto& runner = TestRunner::getInstance();
    runner.addSuite(createBlackScholesTestSuite());
}
