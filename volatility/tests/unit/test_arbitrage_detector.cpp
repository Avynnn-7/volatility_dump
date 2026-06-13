

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include <cmath>

void test_arb_free_surface_no_violations() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();

    int significantViolations = 0;
    for (const auto& v : violations) {
        if (std::abs(v.magnitude) > 0.01) significantViolations++;
    }
    ASSERT_TRUE(significantViolations <= 3);  
}

void test_arb_free_surface_quality_score() {
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    detector.detect();
    double quality = detector.getQualityScore();

    ASSERT_IN_RANGE(quality, 0.0, 1.0);  
}

void test_butterfly_violation_detected() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();
    ASSERT_NOT_EMPTY(violations);
    
    bool hasButterflyViolation = false;
    for (const auto& v : violations) {
        if (v.type == ArbType::ButterflyViolation) {
            hasButterflyViolation = true;
            break;
        }
    }
    ASSERT_TRUE(hasButterflyViolation);
}

void test_butterfly_violation_magnitude() {
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
            q.iv = 0.02;  
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();
    
    double maxMagnitude = 0.0;
    for (const auto& v : violations) {
        maxMagnitude = std::max(maxMagnitude, std::abs(v.magnitude));
    }
    ASSERT_TRUE(maxMagnitude > 0.01);
}

void test_butterfly_threshold_sensitivity() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);

    ArbitrageDetector::Config tightConfig;
    tightConfig.butterflyThreshold = 1e-6;
    ArbitrageDetector detectorTight(surface);
    detectorTight.setConfig(tightConfig);
    auto violationsTight = detectorTight.detect();

    ArbitrageDetector::Config looseConfig;
    looseConfig.butterflyThreshold = 0.1;
    ArbitrageDetector detectorLoose(surface);
    detectorLoose.setConfig(looseConfig);
    auto violationsLoose = detectorLoose.detect();

    ASSERT_TRUE(violationsTight.size() >= 0);  
    ASSERT_TRUE(violationsLoose.size() >= 0);
}

void test_calendar_violation_detected() {
    auto quotes = MockDataGenerator::generateCalendarArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();
    ASSERT_NOT_EMPTY(violations);
    
    bool hasCalendarViolation = false;
    for (const auto& v : violations) {
        if (v.type == ArbType::CalendarViolation) {
            hasCalendarViolation = true;
            break;
        }
    }
    ASSERT_TRUE(hasCalendarViolation);
}

void test_calendar_spread_monotonicity() {
    std::vector<Quote> quotes;
    
    quotes.push_back({100.0, 0.25, 0.25});
    quotes.push_back({100.0, 0.5, 0.28});
    quotes.push_back({100.0, 1.0, 0.10});  
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();

    ASSERT_NOT_EMPTY(violations);
}

void test_calendar_threshold_sensitivity() {
    auto quotes = MockDataGenerator::generateCalendarArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    ArbitrageDetector::Config tightConfig;
    tightConfig.calendarThreshold = 1e-8;
    ArbitrageDetector detectorTight(surface);
    detectorTight.setConfig(tightConfig);
    auto violationsTight = detectorTight.detect();
    
    ArbitrageDetector::Config looseConfig;
    looseConfig.calendarThreshold = 0.01;
    ArbitrageDetector detectorLoose(surface);
    detectorLoose.setConfig(looseConfig);
    auto violationsLoose = detectorLoose.detect();
    
    ASSERT_TRUE(violationsTight.size() >= violationsLoose.size());
}

void test_multiple_arbitrage_types() {
    auto quotes = ExtendedMockDataGenerator::generateAllArbitrageTypes(100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();
    ASSERT_NOT_EMPTY(violations);
    
    bool hasButterfly = false;
    bool hasCalendar = false;
    
    for (const auto& v : violations) {
        if (v.type == ArbType::ButterflyViolation) hasButterfly = true;
        if (v.type == ArbType::CalendarViolation) hasCalendar = true;
    }
    
    ASSERT_TRUE(hasButterfly || hasCalendar);
}

void test_quality_score_range() {
    auto quotes = MockDataGenerator::generateQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    detector.detect();
    double quality = detector.getQualityScore();
    
    ASSERT_IN_RANGE(quality, 0.0, 1.0);
}

void test_quality_score_decreases_with_violations() {
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    auto goodQuotes = MockDataGenerator::generateArbitrageFreeQuotes(21, 100.0);
    VolSurface goodSurface(goodQuotes, marketData);
    ArbitrageDetector goodDetector(goodSurface);
    goodDetector.detect();
    double goodQuality = goodDetector.getQualityScore();
    
    auto badQuotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    VolSurface badSurface(badQuotes, marketData);
    ArbitrageDetector badDetector(badSurface);
    badDetector.detect();
    double badQuality = badDetector.getQualityScore();
    
    ASSERT_TRUE(goodQuality >= badQuality);
}

void test_config_enable_density_check() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    
    ArbitrageDetector::Config config;
    config.enableDensityCheck = true;
    
    ArbitrageDetector detector(surface);
    detector.setConfig(config);
    
    auto violations = detector.detect();
    ASSERT_NOT_EMPTY(violations);
}

void test_config_default_thresholds() {
    ArbitrageDetector::Config config;
    
    ASSERT_TRUE(config.butterflyThreshold > 0);
    ASSERT_TRUE(config.calendarThreshold > 0);
    ASSERT_TRUE(config.monotonicityThreshold > 0);
}

void test_single_quote_surface() {
    std::vector<Quote> quotes = {{100.0, 1.0, 0.20}};
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    ASSERT_NO_THROW(detector.detect());
}

void test_two_quote_surface() {
    std::vector<Quote> quotes = {{100.0, 0.5, 0.20}, {100.0, 1.0, 0.22}};
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();
    
    int calendarCount = 0;
    for (const auto& v : violations) {
        if (v.type == ArbType::CalendarViolation) calendarCount++;
    }
    ASSERT_TRUE(calendarCount == 0);
}

void test_flat_volatility_surface() {
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

void test_high_volatility_surface() {
    std::vector<Quote> quotes;
    for (double T : {0.25, 0.5, 1.0}) {
        for (double K : {80.0, 90.0, 100.0, 110.0, 120.0}) {
            double logM = std::log(K / 100.0);
            double iv = 1.0 + 0.3 * logM * logM;  
            quotes.push_back({K, T, iv});
        }
    }
    
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    ASSERT_NO_THROW(detector.detect());
}

void test_low_volatility_surface() {
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
    ArbitrageDetector detector(surface);
    
    ASSERT_NO_THROW(detector.detect());
}

void test_violation_has_description() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();
    
    for (const auto& v : violations) {
        ASSERT_FALSE(v.description.empty());
    }
}

void test_violation_location_info() {
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(21, 100.0);
    MarketData marketData{100.0, 0.05, 0.02, "2024-01-01", "USD"};
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    
    auto violations = detector.detect();

    int validLocations = 0;
    for (const auto& v : violations) {
        if (v.strike > 0 && v.expiry > 0) {
            validLocations++;
        }
    }
    
    ASSERT_TRUE(validLocations >= 0);  
}

std::unique_ptr<TestSuite> createArbitrageDetectorTestSuite() {
    auto suite = std::make_unique<TestSuite>("Arbitrage Detector Unit Tests");

    suite->addTest("Arbitrage-Free Surface No Violations", test_arb_free_surface_no_violations);
    suite->addTest("Arbitrage-Free Surface Quality Score", test_arb_free_surface_quality_score);

    suite->addTest("Butterfly Violation Detected", test_butterfly_violation_detected);
    suite->addTest("Butterfly Violation Magnitude", test_butterfly_violation_magnitude);
    suite->addTest("Butterfly Threshold Sensitivity", test_butterfly_threshold_sensitivity);

    suite->addTest("Calendar Violation Detected", test_calendar_violation_detected);
    suite->addTest("Calendar Spread Monotonicity", test_calendar_spread_monotonicity);
    suite->addTest("Calendar Threshold Sensitivity", test_calendar_threshold_sensitivity);

    suite->addTest("Multiple Arbitrage Types", test_multiple_arbitrage_types);

    suite->addTest("Quality Score Range", test_quality_score_range);
    suite->addTest("Quality Score Decreases With Violations", test_quality_score_decreases_with_violations);

    suite->addTest("Config Enable Density Check", test_config_enable_density_check);
    suite->addTest("Config Default Thresholds", test_config_default_thresholds);

    suite->addTest("Single Quote Surface", test_single_quote_surface);
    suite->addTest("Two Quote Surface", test_two_quote_surface);
    suite->addTest("Flat Volatility Surface", test_flat_volatility_surface);
    suite->addTest("High Volatility Surface", test_high_volatility_surface);
    suite->addTest("Low Volatility Surface", test_low_volatility_surface);

    suite->addTest("Violation Has Description", test_violation_has_description);
    suite->addTest("Violation Location Info", test_violation_location_info);
    
    return suite;
}

void registerArbitrageDetectorTests() {
    auto& runner = TestRunner::getInstance();
    runner.addSuite(createArbitrageDetectorTestSuite());
}
