

#include "vol_api.hpp"
#include "svi_surface.hpp"
#include "local_vol.hpp"
#include "data_handler.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <queue>
#include <map>
#include <condition_variable>

static const char* API_VERSION = "1.0.0";

std::string ArbitrageCheckResponse::toJson() const {
    nlohmann::json j;
    
    j["arbitrageFree"] = arbitrageFree;
    j["qualityScore"] = qualityScore;
    j["surfaceType"] = surfaceType;
    j["timing"]["detectionTime"] = detectionTime;
    j["timing"]["correctionTime"] = correctionTime;

    j["violations"] = nlohmann::json::array();
    for (const auto& v : violations) {
        nlohmann::json vj;
        
        switch (v.type) {
            case ArbType::ButterflyViolation:
                vj["type"] = "butterfly";
                break;
            case ArbType::CalendarViolation:
                vj["type"] = "calendar";
                break;
            case ArbType::MonotonicityViolation:
                vj["type"] = "monotonicity";
                break;
            case ArbType::VerticalSpreadViolation:
                vj["type"] = "verticalSpread";
                break;
            case ArbType::DensityIntegrityViolation:
                vj["type"] = "densityIntegrity";
                break;
            case ArbType::ExtremeValueViolation:
                vj["type"] = "extremeValue";
                break;
            default:
                vj["type"] = "unknown";
        }
        
        vj["strike"] = v.strike;
        vj["expiry"] = v.expiry;
        vj["magnitude"] = v.magnitude;
        vj["threshold"] = v.threshold;
        vj["description"] = v.description;
        vj["severity"] = v.severityScore();
        vj["isCritical"] = v.isCritical();
        
        j["violations"].push_back(vj);
    }

    if (qpResult.success) {
        j["qpResult"]["success"] = qpResult.success;
        j["qpResult"]["status"] = qpResult.status;
        j["qpResult"]["iterations"] = qpResult.iterations;
        j["qpResult"]["objectiveValue"] = qpResult.objectiveValue;
        j["qpResult"]["regularizationPenalty"] = qpResult.regularizationPenalty;
        j["qpResult"]["solveTime"] = qpResult.solveTime;
    }
    
    return j.dump();
}

CachedVolSurface::CachedVolSurface(const std::vector<Quote>& quotes, 
                                   const MarketData& marketData)
    : surface_(std::make_unique<VolSurface>(quotes, marketData))
    , cacheHits_(0)
    , cacheMisses_(0)
    , maxCacheSize_(10000)
{
}

double CachedVolSurface::impliedVol(double strike, double expiry) const {
    CacheKey key{strike, expiry};
    
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            ++cacheHits_;
            return it->second;
        }
    }
    
    double vol = surface_->impliedVol(strike, expiry);
    
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        ++cacheMisses_;
        
        if (cache_.size() >= maxCacheSize_) {
            auto it = cache_.begin();
            size_t toRemove = cache_.size() / 2;
            for (size_t i = 0; i < toRemove && it != cache_.end(); ++i) {
                it = cache_.erase(it);
            }
        }
        
        cache_[key] = vol;
    }
    
    return vol;
}

void CachedVolSurface::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_.clear();
    cacheHits_ = 0;
    cacheMisses_ = 0;
}

size_t CachedVolSurface::getCacheSize() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.size();
}

void CachedVolSurface::setCacheSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    maxCacheSize_ = maxSize;
    
    while (cache_.size() > maxCacheSize_ && !cache_.empty()) {
        cache_.erase(cache_.begin());
    }
}

double CachedVolSurface::getCacheHitRate() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    size_t total = cacheHits_ + cacheMisses_;
    if (total == 0) return 0.0;
    return static_cast<double>(cacheHits_) / total;
}

void CachedVolSurface::resetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cacheHits_ = 0;
    cacheMisses_ = 0;
}

ThreadSafeArbitrageDetector::ThreadSafeArbitrageDetector(const VolSurface& surface)
    : detector_(std::make_unique<ArbitrageDetector>(surface))
{
}

std::vector<ArbViolation> ThreadSafeArbitrageDetector::detect() const {
    std::lock_guard<std::mutex> lock(detectorMutex_);
    return detector_->detect();
}

std::vector<std::vector<ArbViolation>> ThreadSafeArbitrageDetector::detectBatch(
    const std::vector<VolSurface>& surfaces) const 
{
    std::vector<std::vector<ArbViolation>> results;
    results.reserve(surfaces.size());
    
    for (const auto& surface : surfaces) {
        ArbitrageDetector detector(surface);
        results.push_back(detector.detect());
    }
    
    return results;
}

VolatilityArbitrageAPI& VolatilityArbitrageAPI::getInstance() {
    static VolatilityArbitrageAPI instance;
    return instance;
}

ApiResponse VolatilityArbitrageAPI::checkArbitrage(const ArbitrageCheckRequest& request) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        ArbitrageCheckResponse response = processRequestInternal(request);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            metrics_.totalProcessingTime += execTime;
            metrics_.totalRequests++;
            metrics_.successfulRequests++;
            metrics_.averageResponseTime = metrics_.totalProcessingTime / metrics_.totalRequests;
            metrics_.operationTimes["checkArbitrage"] += execTime;
        }
        
        return ApiResponse(true, "Arbitrage check completed", response.toJson(), execTime);
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            metrics_.totalProcessingTime += execTime;
            metrics_.totalRequests++;
        }
        
        return ApiResponse(false, std::string("Error: ") + e.what(), "", execTime);
    }
}

ApiResponse VolatilityArbitrageAPI::correctSurface(const ArbitrageCheckRequest& request) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        VolSurface surface(request.quotes, request.marketData);
        qpSolver_.setConfig(request.qpConfig);
        QPResult result = qpSolver_.solve(surface);
        
        if (!result.success) {
            throw std::runtime_error("QP solver failed: " + result.status);
        }
        
        VolSurface corrected = qpSolver_.buildCorrectedSurface(surface, result);
        ArbitrageDetector verifier(corrected);
        auto violations = verifier.detect();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        nlohmann::json responseJson;
        responseJson["success"] = true;
        responseJson["qpStatus"] = result.status;
        responseJson["iterations"] = result.iterations;
        responseJson["objectiveValue"] = result.objectiveValue;
        responseJson["solveTime"] = result.solveTime;
        responseJson["remainingViolations"] = static_cast<int>(violations.size());
        responseJson["qualityScore"] = verifier.getQualityScore();
        
        nlohmann::json ivGrid = nlohmann::json::array();
        for (int i = 0; i < result.ivFlat.size(); ++i) {
            ivGrid.push_back(result.ivFlat(i));
        }
        responseJson["correctedIV"] = ivGrid;
        
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            metrics_.totalProcessingTime += execTime;
            metrics_.totalRequests++;
            metrics_.successfulRequests++;
            metrics_.operationTimes["correctSurface"] += execTime;
        }
        
        return ApiResponse(true, "Surface corrected successfully", responseJson.dump(), execTime);
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        return ApiResponse(false, std::string("Correction failed: ") + e.what(), "", execTime);
    }
}

ApiResponse VolatilityArbitrageAPI::analyzeQuality(const ArbitrageCheckRequest& request) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        VolSurface surface(request.quotes, request.marketData);
        ArbitrageDetector detector(surface);
        detector.setConfig(request.arbConfig);
        
        auto violations = detector.detect();
        double qualityScore = detector.getQualityScore();
        
        nlohmann::json analysis;
        analysis["qualityScore"] = qualityScore;
        analysis["totalViolations"] = static_cast<int>(violations.size());
        analysis["isArbitrageFree"] = violations.empty();
        
        int butterflyCount = 0, calendarCount = 0, otherCount = 0;
        double maxSeverity = 0.0;
        
        for (const auto& v : violations) {
            maxSeverity = std::max(maxSeverity, std::abs(v.magnitude));
            switch (v.type) {
                case ArbType::ButterflyViolation: ++butterflyCount; break;
                case ArbType::CalendarViolation: ++calendarCount; break;
                default: ++otherCount; break;
            }
        }
        
        analysis["butterflyViolations"] = butterflyCount;
        analysis["calendarViolations"] = calendarCount;
        analysis["otherViolations"] = otherCount;
        analysis["maxSeverity"] = maxSeverity;
        analysis["numStrikes"] = static_cast<int>(surface.strikes().size());
        analysis["numExpiries"] = static_cast<int>(surface.expiries().size());
        analysis["spotPrice"] = surface.spot();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        return ApiResponse(true, "Quality analysis completed", analysis.dump(), execTime);
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        return ApiResponse(false, std::string("Analysis failed: ") + e.what(), "", execTime);
    }
}

ApiResponse VolatilityArbitrageAPI::batchCheckArbitrage(
    const std::vector<ArbitrageCheckRequest>& requests) 
{
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        nlohmann::json results = nlohmann::json::array();
        int successCount = 0;
        int failCount = 0;
        
        for (const auto& request : requests) {
            try {
                auto response = processRequestInternal(request);
                nlohmann::json item;
                item["success"] = true;
                item["arbitrageFree"] = response.arbitrageFree;
                item["violationCount"] = static_cast<int>(response.violations.size());
                item["qualityScore"] = response.qualityScore;
                results.push_back(item);
                ++successCount;
            } catch (const std::exception& e) {
                nlohmann::json item;
                item["success"] = false;
                item["error"] = e.what();
                results.push_back(item);
                ++failCount;
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        nlohmann::json response;
        response["totalRequests"] = static_cast<int>(requests.size());
        response["successCount"] = successCount;
        response["failCount"] = failCount;
        response["results"] = results;
        
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            metrics_.totalProcessingTime += execTime;
            metrics_.totalRequests += static_cast<int>(requests.size());
            metrics_.successfulRequests += successCount;
            metrics_.operationTimes["batchCheckArbitrage"] += execTime;
        }
        
        return ApiResponse(true, "Batch processing completed", response.dump(), execTime);
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        return ApiResponse(false, std::string("Batch failed: ") + e.what(), "", execTime);
    }
}

void VolatilityArbitrageAPI::startRealTimeProcessing(
    std::function<void(const ArbitrageCheckRequest&)> callback) 
{
    if (processingActive_.exchange(true)) {
        return;
    }
    
    processingThread_ = std::thread([this, callback]() {
        while (processingActive_) {
            ArbitrageCheckRequest request;
            if (requestQueue_.pop(request)) {
                try {
                    callback(request);
                } catch (...) {
                    // Ignore callback exceptions in real-time mode
                }
            } else {
                std::this_thread::yield();
            }
        }
    });
}

void VolatilityArbitrageAPI::stopRealTimeProcessing() {
    processingActive_ = false;
    
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
}

bool VolatilityArbitrageAPI::enqueueRealTimeRequest(const ArbitrageCheckRequest& request) {
    if (!processingActive_) return false;
    return requestQueue_.push(request);
}

ApiResponse VolatilityArbitrageAPI::updateConfiguration(const std::string& configJson) {
    try {
        nlohmann::json j = nlohmann::json::parse(configJson);
        
        std::lock_guard<std::mutex> lock(configMutex_);
        
        if (j.contains("arbitrageDetection")) {
            auto& arb = j["arbitrageDetection"];
            if (arb.contains("butterflyThreshold"))
                defaultArbConfig_.butterflyThreshold = arb["butterflyThreshold"].get<double>();
            if (arb.contains("calendarThreshold"))
                defaultArbConfig_.calendarThreshold = arb["calendarThreshold"].get<double>();
            if (arb.contains("enableDensityCheck"))
                defaultArbConfig_.enableDensityCheck = arb["enableDensityCheck"].get<bool>();
        }
        
        if (j.contains("qpSolver")) {
            auto& qp = j["qpSolver"];
            if (qp.contains("regularizationWeight"))
                defaultQPConfig_.regularizationWeight = qp["regularizationWeight"].get<double>();
            if (qp.contains("maxIterations"))
                defaultQPConfig_.maxIterations = qp["maxIterations"].get<int>();
            if (qp.contains("tolerance"))
                defaultQPConfig_.tolerance = qp["tolerance"].get<double>();
        }
        
        return ApiResponse(true, "Configuration updated", "{}", 0.0);
        
    } catch (const std::exception& e) {
        return ApiResponse(false, std::string("Config update failed: ") + e.what(), "", 0.0);
    }
}

ApiResponse VolatilityArbitrageAPI::getConfiguration() {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    nlohmann::json config;
    
    config["arbitrageDetection"]["butterflyThreshold"] = defaultArbConfig_.butterflyThreshold;
    config["arbitrageDetection"]["calendarThreshold"] = defaultArbConfig_.calendarThreshold;
    config["arbitrageDetection"]["monotonicityThreshold"] = defaultArbConfig_.monotonicityThreshold;
    config["arbitrageDetection"]["enableDensityCheck"] = defaultArbConfig_.enableDensityCheck;
    
    config["qpSolver"]["regularizationWeight"] = defaultQPConfig_.regularizationWeight;
    config["qpSolver"]["smoothnessWeight"] = defaultQPConfig_.smoothnessWeight;
    config["qpSolver"]["maxIterations"] = defaultQPConfig_.maxIterations;
    config["qpSolver"]["tolerance"] = defaultQPConfig_.tolerance;
    config["qpSolver"]["minVol"] = defaultQPConfig_.minVol;
    config["qpSolver"]["maxVol"] = defaultQPConfig_.maxVol;
    
    return ApiResponse(true, "Current configuration", config.dump(2), 0.0);
}

ApiResponse VolatilityArbitrageAPI::getStatus() {
    nlohmann::json status;
    
    status["status"] = "healthy";
    status["version"] = API_VERSION;
    status["realTimeProcessing"] = processingActive_.load();
    
    {
        // lock-free size approximation
        status["queueSize"] = 0; // SPSCBuffer doesn't have size() for lock-free reasons
    }
    
    return ApiResponse(true, "System status", status.dump(), 0.0);
}

ApiResponse VolatilityArbitrageAPI::getPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    nlohmann::json metricsJson;
    metricsJson["totalProcessingTime"] = metrics_.totalProcessingTime;
    metricsJson["totalRequests"] = metrics_.totalRequests;
    metricsJson["successfulRequests"] = metrics_.successfulRequests;
    metricsJson["failedRequests"] = metrics_.totalRequests - metrics_.successfulRequests;
    metricsJson["averageResponseTime"] = metrics_.averageResponseTime;
    metricsJson["successRate"] = metrics_.totalRequests > 0 
        ? static_cast<double>(metrics_.successfulRequests) / metrics_.totalRequests 
        : 1.0;
    
    nlohmann::json opTimes;
    for (const auto& [op, time] : metrics_.operationTimes) {
        opTimes[op] = time;
    }
    metricsJson["operationTimes"] = opTimes;
    
    return ApiResponse(true, "Performance metrics", metricsJson.dump(2), 0.0);
}

ApiResponse VolatilityArbitrageAPI::loadData(const std::string& dataSource) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        DataHandler::Config config;
        config.filePath = dataSource;
        
        if (dataSource.find(".json") != std::string::npos) {
            config.source = DataSource::JSON_FILE;
        } else if (dataSource.find(".csv") != std::string::npos) {
            config.source = DataSource::CSV_FILE;
        }
        
        DataHandler handler(config);
        auto [quotes, marketData] = handler.loadData();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        nlohmann::json result;
        result["quotesLoaded"] = static_cast<int>(quotes.size());
        result["spotPrice"] = marketData.spot;
        result["riskFreeRate"] = marketData.riskFreeRate;
        result["dividendYield"] = marketData.dividendYield;
        
        auto quality = handler.getQualityMetrics();
        result["dataQuality"]["totalQuotes"] = quality.totalQuotes;
        result["dataQuality"]["validQuotes"] = quality.validQuotes;
        result["dataQuality"]["overallQuality"] = quality.getOverallQuality();
        
        return ApiResponse(true, "Data loaded successfully", result.dump(), execTime);
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        double execTime = std::chrono::duration<double>(endTime - startTime).count();
        
        return ApiResponse(false, std::string("Load failed: ") + e.what(), "", execTime);
    }
}

ApiResponse VolatilityArbitrageAPI::exportData(const std::string& , 
                                               const std::string& ) 
{
    return ApiResponse(false, "Export not implemented", "", 0.0);
}

bool VolatilityArbitrageAPI::healthCheck() {
    try {
        std::vector<Quote> testQuotes = {{100.0, 0.25, 0.20}};
        MarketData testData{100.0, 0.05, 0.0, "2024-01-01", "USD"};
        VolSurface testSurface(testQuotes, testData);
        double vol = testSurface.impliedVol(100.0, 0.25);
        return std::isfinite(vol) && vol > 0;
    } catch (...) {
        return false;
    }
}

std::string VolatilityArbitrageAPI::getVersion() {
    return API_VERSION;
}

ArbitrageCheckResponse VolatilityArbitrageAPI::processRequestInternal(
    const ArbitrageCheckRequest& request) 
{
    if (request.interpolationMethod == "raw" || request.interpolationMethod == "orderbook") {
        return processRawOrderbook(request);
    } else if (request.interpolationMethod == "svi") {
        return processWithSVI(request);
    } else {
        return processWithBilinear(request);
    }
}

ArbitrageCheckResponse VolatilityArbitrageAPI::processWithBilinear(
    const ArbitrageCheckRequest& request) 
{
    ArbitrageCheckResponse response;
    response.surfaceType = "bilinear";
    
    auto detectStart = std::chrono::high_resolution_clock::now();
    
    VolSurface surface(request.quotes, request.marketData);
    ArbitrageDetector detector(surface);
    detector.setConfig(request.arbConfig);
    response.violations = detector.detect();
    response.arbitrageFree = response.violations.empty();
    response.qualityScore = detector.getQualityScore();
    
    auto detectEnd = std::chrono::high_resolution_clock::now();
    response.detectionTime = std::chrono::duration<double>(detectEnd - detectStart).count();
    
    if (request.enableQPCorrection && !response.arbitrageFree) {
        auto correctStart = std::chrono::high_resolution_clock::now();
        
        qpSolver_.setConfig(request.qpConfig);
        response.qpResult = qpSolver_.solve(surface);
        
        auto correctEnd = std::chrono::high_resolution_clock::now();
        response.correctionTime = std::chrono::duration<double>(correctEnd - correctStart).count();
    }
    
    return response;
}

ArbitrageCheckResponse VolatilityArbitrageAPI::processWithSVI(
    const ArbitrageCheckRequest& request) 
{
    ArbitrageCheckResponse response;
    response.surfaceType = "svi";
    
    auto detectStart = std::chrono::high_resolution_clock::now();
    
    SVISurface sviSurface(request.quotes, request.marketData);
    auto sviViolations = sviSurface.getArbitrageViolations();
    response.arbitrageFree = sviViolations.empty();
    response.qualityScore = sviSurface.isArbitrageFree() ? 1.0 : 0.5;
    
    for (const auto& desc : sviViolations) {
        ArbViolation v;
        v.type = ArbType::ButterflyViolation;
        v.strike = 0;
        v.expiry = 0;
        v.magnitude = 0;
        v.description = desc;
        response.violations.push_back(v);
    }
    
    auto detectEnd = std::chrono::high_resolution_clock::now();
    response.detectionTime = std::chrono::duration<double>(detectEnd - detectStart).count();
    response.correctionTime = 0.0;
    
    return response;
}

ArbitrageCheckResponse VolatilityArbitrageAPI::processRawOrderbook(
    const ArbitrageCheckRequest& request)
{
    ArbitrageCheckResponse response;
    response.surfaceType = "raw";
    
    auto detectStart = std::chrono::high_resolution_clock::now();
    
    RawArbitrageScanner::Config config;
    config.r = request.marketData.riskFreeRate;
    RawArbitrageScanner scanner(config);
    auto scanResult = scanner.scan(request.quotes, request.marketData);
    
    response.violations = scanResult.violations;
    response.arbitrageFree = response.violations.empty();
    
    if (response.violations.empty()) {
        response.qualityScore = 1.0;
    } else {
        response.qualityScore = std::max(0.0, 1.0 - response.violations.size() * 0.1);
    }
    
    auto detectEnd = std::chrono::high_resolution_clock::now();
    response.detectionTime = std::chrono::duration<double>(detectEnd - detectStart).count();
    response.correctionTime = 0.0;
    response.qpResult.success = false;
    
    return response;
}

std::string RestAPIHandler::handlePostArbitrageCheck(const std::string& requestBody) {
    try {
        ArbitrageCheckRequest request = parseArbitrageRequest(requestBody);
        ApiResponse response = api_.checkArbitrage(request);
        return serializeResponse(response);
    } catch (const std::exception& e) {
        return createErrorResponse(e.what(), 400);
    }
}

std::string RestAPIHandler::handleGetStatus() {
    ApiResponse response = api_.getStatus();
    return serializeResponse(response);
}

std::string RestAPIHandler::handleGetConfig() {
    ApiResponse response = api_.getConfiguration();
    return serializeResponse(response);
}

std::string RestAPIHandler::handlePostConfig(const std::string& requestBody) {
    ApiResponse response = api_.updateConfiguration(requestBody);
    return serializeResponse(response);
}

std::string RestAPIHandler::createErrorResponse(const std::string& error, int httpCode) {
    nlohmann::json response;
    response["success"] = false;
    response["error"]["code"] = httpCode;
    response["error"]["message"] = error;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return response.dump();
}

std::string RestAPIHandler::createSuccessResponse(const std::string& data) {
    nlohmann::json response;
    response["success"] = true;
    response["data"] = nlohmann::json::parse(data);
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return response.dump();
}

ArbitrageCheckRequest RestAPIHandler::parseArbitrageRequest(const std::string& json) {
    nlohmann::json j = nlohmann::json::parse(json);
    ArbitrageCheckRequest request;
    
    if (!j.contains("quotes") || !j["quotes"].is_array()) {
        throw std::invalid_argument("Missing or invalid 'quotes' array");
    }
    
    for (const auto& q : j["quotes"]) {
        Quote quote;
        quote.strike = q.at("strike").get<double>();
        quote.expiry = q.at("expiry").get<double>();
        quote.iv = q.at("iv").get<double>();
        quote.bid = q.value("bid", 0.0);
        quote.ask = q.value("ask", 0.0);
        quote.volume = q.value("volume", 0.0);
        
        std::string typeStr = q.value("optionType", q.value("type", "CALL"));
        quote.optionType = (typeStr == "PUT" || typeStr == "PE" || typeStr == "put" || typeStr == "PUT_OPTIONS") 
                            ? QuoteOptionType::PUT : QuoteOptionType::CALL;

        request.quotes.push_back(quote);
    }
    
    if (!j.contains("marketData")) {
        throw std::invalid_argument("Missing 'marketData' object");
    }
    
    auto& md = j["marketData"];
    request.marketData.spot = md.at("spot").get<double>();
    request.marketData.riskFreeRate = md.value("riskFreeRate", 0.05);
    request.marketData.dividendYield = md.value("dividendYield", 0.0);
    request.marketData.valuationDate = md.value("valuationDate", "2024-01-01");
    request.marketData.currency = md.value("currency", "USD");
    
    request.interpolationMethod = j.value("interpolationMethod", "bilinear");
    request.enableQPCorrection = j.value("enableQPCorrection", true);
    
    if (j.contains("qpConfig")) {
        auto& qp = j["qpConfig"];
        request.qpConfig.regularizationWeight = qp.value("regularizationWeight", 1e-6);
        request.qpConfig.smoothnessWeight = qp.value("smoothnessWeight", 1e-4);
        request.qpConfig.maxIterations = qp.value("maxIterations", 10000);
        request.qpConfig.tolerance = qp.value("tolerance", 1e-9);
    }
    
    if (j.contains("arbConfig")) {
        auto& arb = j["arbConfig"];
        request.arbConfig.butterflyThreshold = arb.value("butterflyThreshold", 1e-6);
        request.arbConfig.calendarThreshold = arb.value("calendarThreshold", 1e-6);
        request.arbConfig.enableDensityCheck = arb.value("enableDensityCheck", true);
    }
    
    return request;
}

std::string RestAPIHandler::serializeResponse(const ApiResponse& response) {
    nlohmann::json j;
    j["success"] = response.success;
    j["message"] = response.message;
    j["executionTime"] = response.executionTime;
    
    if (!response.data.empty()) {
        try {
            j["data"] = nlohmann::json::parse(response.data);
        } catch (...) {
            j["data"] = response.data;
        }
    }
    
    return j.dump(2);
}

void PerformanceOptimizer::vectorizedBlackScholes(
    const double* S, const double* K, const double* T, 
    const double* sigma, const double* r, const double* q,
    double* prices, size_t n) 
{
    for (size_t i = 0; i < n; ++i) {
        if (T[i] <= 0.0 || sigma[i] <= 0.0) {
            prices[i] = std::max(S[i] - K[i], 0.0);
            continue;
        }
        
        double sqrtT = std::sqrt(T[i]);
        double d1 = (std::log(S[i] / K[i]) + (r[i] - q[i] + 0.5 * sigma[i] * sigma[i]) * T[i]) 
                    / (sigma[i] * sqrtT);
        double d2 = d1 - sigma[i] * sqrtT;
        
        auto normalCDF = [](double x) {
            return 0.5 * std::erfc(-x / std::sqrt(2.0));
        };
        
        prices[i] = S[i] * std::exp(-q[i] * T[i]) * normalCDF(d1) 
                  - K[i] * std::exp(-r[i] * T[i]) * normalCDF(d2);
    }
}

namespace VolApi {

std::string detect_arbitrage(const std::string& json_input) {
    RestAPIHandler handler;
    return handler.handlePostArbitrageCheck(json_input);
}

std::string repair_surface(const std::string& json_input) {
    try {
        RestAPIHandler handler;
        ArbitrageCheckRequest request = handler.parseArbitrageRequest(json_input);
        request.enableQPCorrection = true;
        
        auto& api = VolatilityArbitrageAPI::getInstance();
        ApiResponse response = api.correctSurface(request);
        
        return handler.serializeResponse(response);
        
    } catch (const std::exception& e) {
        RestAPIHandler handler;
        return handler.createErrorResponse(e.what(), 400);
    }
}

std::string compute_local_vol(const std::string& json_input) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_input);
        
        std::vector<Quote> quotes;
        for (const auto& q : j["quotes"]) {
            quotes.push_back({
                q["strike"].get<double>(),
                q["expiry"].get<double>(),
                q["iv"].get<double>()
            });
        }
        
        MarketData marketData;
        marketData.spot = j["marketData"]["spot"].get<double>();
        marketData.riskFreeRate = j["marketData"].value("riskFreeRate", 0.05);
        marketData.dividendYield = j["marketData"].value("dividendYield", 0.0);
        marketData.valuationDate = j["marketData"].value("valuationDate", "2024-01-01");
        marketData.currency = j["marketData"].value("currency", "USD");
        
        VolSurface surface(quotes, marketData);
        LocalVolSurface localVol(surface);
        
        nlohmann::json response;
        response["success"] = true;
        response["allPositive"] = localVol.allPositive();
        
        const auto& grid = localVol.localVolGrid();
        response["grid"] = nlohmann::json::array();
        for (int i = 0; i < grid.rows(); ++i) {
            nlohmann::json row = nlohmann::json::array();
            for (int jIdx = 0; jIdx < grid.cols(); ++jIdx) {
                row.push_back(grid(i, jIdx));
            }
            response["grid"].push_back(row);
        }
        
        response["strikes"] = surface.strikes();
        response["expiries"] = surface.expiries();
        
        return response.dump();
        
    } catch (const std::exception& e) {
        nlohmann::json error;
        error["success"] = false;
        error["error"] = e.what();
        return error.dump();
    }
}

std::string fit_svi(const std::string& json_input) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_input);
        
        std::vector<Quote> quotes;
        for (const auto& q : j["quotes"]) {
            quotes.push_back({
                q["strike"].get<double>(),
                q["expiry"].get<double>(),
                q["iv"].get<double>()
            });
        }
        
        MarketData marketData;
        marketData.spot = j["marketData"]["spot"].get<double>();
        marketData.riskFreeRate = j["marketData"].value("riskFreeRate", 0.05);
        marketData.dividendYield = j["marketData"].value("dividendYield", 0.0);
        marketData.valuationDate = j["marketData"].value("valuationDate", "2024-01-01");
        marketData.currency = j["marketData"].value("currency", "USD");
        
        SVISurface sviSurface(quotes, marketData);
        
        nlohmann::json response;
        response["success"] = true;
        response["isArbitrageFree"] = sviSurface.isArbitrageFree();
        
        response["parameters"] = nlohmann::json::array();
        const auto& expiries = sviSurface.expiries();
        const auto& params = sviSurface.sviParams();
        
        for (size_t i = 0; i < expiries.size(); ++i) {
            nlohmann::json p;
            p["expiry"] = expiries[i];
            p["a"] = params[i].a;
            p["b"] = params[i].b;
            p["rho"] = params[i].rho;
            p["m"] = params[i].m;
            p["sigma"] = params[i].sigma;
            p["isValid"] = params[i].isValid();
            response["parameters"].push_back(p);
        }
        
        auto violations = sviSurface.getArbitrageViolations();
        response["violations"] = violations;
        
        return response.dump();
        
    } catch (const std::exception& e) {
        nlohmann::json error;
        error["success"] = false;
        error["error"] = e.what();
        return error.dump();
    }
}

} 

