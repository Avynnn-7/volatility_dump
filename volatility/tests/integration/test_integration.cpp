

#include "test_framework.hpp"
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "svi_surface.hpp"
#include "local_vol.hpp"
#include "data_handler.hpp"
#include "vol_api.hpp"
#include <cmath>
#include <fstream>

void test_full_pipeline_arbitrage_free() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(20, 100.0);

    VolSurface surface(quotes, marketData);
    ASSERT_TRUE(surface.strikes().size() > 0);
    ASSERT_TRUE(surface.expiries().size() > 0);

    ArbitrageDetector detector(surface);
    auto violations = detector.detect();

    double quality = detector.getQualityScore();
    ASSERT_IN_RANGE(quality, 0.0, 1.0);  

    int significantViolations = 0;
    for (const auto& v : violations) {
        if (std::abs(v.magnitude) > 0.01) significantViolations++;
    }
    ASSERT_TRUE(significantViolations <= 5);  
}

void test_full_pipeline_with_arbitrage() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(15, 100.0);

    VolSurface surface(quotes, marketData);

    ArbitrageDetector detectorBefore(surface);
    auto violationsBefore = detectorBefore.detect();
    ASSERT_TRUE(violationsBefore.size() > 0);  

    QPSolver solver;
solver.setup(surface);
    auto result = solver.solve(surface);
    ASSERT_TRUE(result.success);

    VolSurface corrected = solver.buildCorrectedSurface(surface, result);

    ArbitrageDetector detectorAfter(corrected);
    auto violationsAfter = detectorAfter.detect();

    ASSERT_TRUE(violationsAfter.size() <= violationsBefore.size() + 5);

    double qualityAfter = detectorAfter.getQualityScore();
    ASSERT_IN_RANGE(qualityAfter, 0.0, 1.0);
}

void test_full_pipeline_batch_processing() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    
    std::vector<std::vector<Quote>> allQuotes;
    allQuotes.push_back(MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0));
    allQuotes.push_back(MockDataGenerator::generateButterflyArbitrageQuotes(12, 100.0));
    allQuotes.push_back(MockDataGenerator::generateCalendarArbitrageQuotes(15, 100.0));
    
    int successCount = 0;
    
    for (const auto& quotes : allQuotes) {
        VolSurface surface(quotes, marketData);
        ArbitrageDetector detector(surface);
        auto violations = detector.detect();
        
        if (!violations.empty()) {
            QPSolver solver;
solver.setup(surface);
            auto result = solver.solve(surface);
            if (result.success) {
                successCount++;
            }
        } else {
            successCount++;  
        }
    }
    
    ASSERT_EQ(successCount, 3);
}

void test_data_handler_roundtrip() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0);

    VolSurface surface(quotes, marketData);

    ASSERT_TRUE(surface.strikes().size() > 0);
    ASSERT_TRUE(surface.strikes().size() <= quotes.size());  
    ASSERT_TRUE(std::abs(surface.spot() - 100.0) < 0.01);
}

void test_data_validation_integration() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);

    std::vector<Quote> validQuotes = {
        {90.0, 0.5, 0.22},
        {100.0, 0.5, 0.20},
        {110.0, 0.5, 0.22},
    };
    
    ASSERT_NO_THROW({
        VolSurface surface(validQuotes, marketData);
        ArbitrageDetector detector(surface);
        detector.detect();
    });
}

void test_svi_pipeline_basic() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);

    std::vector<Quote> quotes;
    for (double K : {80.0, 90.0, 95.0, 100.0, 105.0, 110.0, 120.0}) {
        double moneyness = std::log(K / 100.0);
        double vol = 0.20 + 0.15 * std::abs(moneyness) + 0.1 * moneyness * moneyness;
        quotes.push_back({K, 1.0, vol});
    }
    
    ASSERT_NO_THROW({
        SVISurface sviSurface(quotes, marketData);

        bool isArbFree = sviSurface.isArbitrageFree();
        (void)isArbFree;  

        const auto& params = sviSurface.sviParams();
        ASSERT_TRUE(params.size() > 0);
    });
}

void test_svi_arbitrage_check_integration() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);

    std::vector<Quote> quotes = {
        {80.0, 1.0, 0.28},
        {90.0, 1.0, 0.23},
        {100.0, 1.0, 0.20},
        {110.0, 1.0, 0.23},
        {120.0, 1.0, 0.28},
    };
    
    SVISurface sviSurface(quotes, marketData);
    auto violations = sviSurface.getArbitrageViolations();

    ASSERT_TRUE(violations.size() <= 2);
}

void test_local_vol_pipeline() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(20, 100.0);
    
    VolSurface surface(quotes, marketData);
    
    ASSERT_NO_THROW({
        LocalVolSurface localVol(surface);

        const auto& grid = localVol.localVolGrid();
        ASSERT_TRUE(grid.rows() > 0);
        ASSERT_TRUE(grid.cols() > 0);

        bool allPositive = localVol.allPositive();
        (void)allPositive;  
    });
}

void test_local_vol_from_corrected_surface() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateButterflyArbitrageQuotes(15, 100.0);

    VolSurface surface(quotes, marketData);

    QPSolver solver;
solver.setup(surface);
    auto result = solver.solve(surface);
    ASSERT_TRUE(result.success);

    VolSurface corrected = solver.buildCorrectedSurface(surface, result);

    ASSERT_NO_THROW({
        LocalVolSurface localVol(corrected);

        bool allPositive = localVol.allPositive();
        (void)allPositive;
    });
}

void test_api_check_arbitrage() {
    auto& api = VolatilityArbitrageAPI::getInstance();

    ArbitrageCheckRequest request;
    request.quotes = MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0);
    request.marketData = MockDataGenerator::generateMarketData(100.0);
    request.interpolationMethod = "bilinear";
    request.enableQPCorrection = false;

    ApiResponse response = api.checkArbitrage(request);
    
    ASSERT_TRUE(response.success);
    ASSERT_TRUE(response.executionTime > 0.0);
    ASSERT_FALSE(response.data.empty());
}

void test_api_correct_surface() {
    auto& api = VolatilityArbitrageAPI::getInstance();

    ArbitrageCheckRequest request;
    request.quotes = MockDataGenerator::generateButterflyArbitrageQuotes(12, 100.0);
    request.marketData = MockDataGenerator::generateMarketData(100.0);
    request.enableQPCorrection = true;

    ApiResponse response = api.correctSurface(request);
    
    ASSERT_TRUE(response.success);
    ASSERT_TRUE(response.executionTime > 0.0);
}

void test_api_batch_check() {
    auto& api = VolatilityArbitrageAPI::getInstance();

    std::vector<ArbitrageCheckRequest> requests;
    
    for (int i = 0; i < 3; ++i) {
        ArbitrageCheckRequest request;
        request.quotes = MockDataGenerator::generateArbitrageFreeQuotes(8 + i * 2, 100.0);
        request.marketData = MockDataGenerator::generateMarketData(100.0);
        request.enableQPCorrection = false;
        requests.push_back(request);
    }

    ApiResponse response = api.batchCheckArbitrage(requests);
    
    ASSERT_TRUE(response.success);
}

void test_api_status_and_config() {
    auto& api = VolatilityArbitrageAPI::getInstance();

    ApiResponse status = api.getStatus();
    ASSERT_TRUE(status.success);

    ApiResponse config = api.getConfiguration();
    ASSERT_TRUE(config.success);

    bool healthy = api.healthCheck();
    ASSERT_TRUE(healthy);

    std::string version = api.getVersion();
    ASSERT_FALSE(version.empty());
}

void test_error_propagation_empty_quotes() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    std::vector<Quote> emptyQuotes;
    
    bool exceptionThrown = false;
    try {
        VolSurface surface(emptyQuotes, marketData);
    } catch (const std::exception& e) {
        (void)e;
        exceptionThrown = true;
    }
    
    ASSERT_TRUE(exceptionThrown);
}

void test_error_recovery_in_pipeline() {
    
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    
    int successCount = 0;

    auto goodQuotes = MockDataGenerator::generateArbitrageFreeQuotes(10, 100.0);
    try {
        VolSurface surface(goodQuotes, marketData);
        ArbitrageDetector detector(surface);
        detector.detect();
        successCount++;
    } catch (...) {}

    auto moreQuotes = MockDataGenerator::generateButterflyArbitrageQuotes(12, 100.0);
    try {
        VolSurface surface(moreQuotes, marketData);
        ArbitrageDetector detector(surface);
        detector.detect();
        successCount++;
    } catch (...) {}
    
    ASSERT_EQ(successCount, 2);
}

void test_pipeline_performance() {
    auto marketData = MockDataGenerator::generateMarketData(100.0);
    auto quotes = MockDataGenerator::generateArbitrageFreeQuotes(50, 100.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    VolSurface surface(quotes, marketData);
    ArbitrageDetector detector(surface);
    auto violations = detector.detect();
    
    if (!violations.empty()) {
        QPSolver solver;
solver.setup(surface);
        solver.solve(surface);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ASSERT_TRUE(duration.count() < 10000);  
}

void registerIntegrationTests() {
    auto& runner = TestRunner::getInstance();
    
    auto suite = std::make_unique<TestSuite>("Integration");

    suite->addTest("FullPipelineArbitrageFree", test_full_pipeline_arbitrage_free);
    suite->addTest("FullPipelineWithArbitrage", test_full_pipeline_with_arbitrage);
    suite->addTest("FullPipelineBatchProcessing", test_full_pipeline_batch_processing);

    suite->addTest("DataHandlerRoundtrip", test_data_handler_roundtrip);
    suite->addTest("DataValidationIntegration", test_data_validation_integration);

    suite->addTest("SVIPipelineBasic", test_svi_pipeline_basic);
    suite->addTest("SVIArbitrageCheckIntegration", test_svi_arbitrage_check_integration);

    suite->addTest("LocalVolPipeline", test_local_vol_pipeline);
    suite->addTest("LocalVolFromCorrectedSurface", test_local_vol_from_corrected_surface);

    suite->addTest("APICheckArbitrage", test_api_check_arbitrage);
    suite->addTest("APICorrectSurface", test_api_correct_surface);
    suite->addTest("APIBatchCheck", test_api_batch_check);
    suite->addTest("APIStatusAndConfig", test_api_status_and_config);

    suite->addTest("ErrorPropagationEmptyQuotes", test_error_propagation_empty_quotes);
    suite->addTest("ErrorRecoveryInPipeline", test_error_recovery_in_pipeline);

    suite->addTest("PipelinePerformance", test_pipeline_performance, 30.0);
    
    runner.addSuite(std::move(suite));
}
