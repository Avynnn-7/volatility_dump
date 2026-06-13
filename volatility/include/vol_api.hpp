

#pragma once
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "data_handler.hpp"
#include "svi_surface.hpp"
#include "raw_arbitrage_scanner.hpp"
#include "logger.hpp"
#include "config_manager.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <map>
#include "spsc_buffer.hpp"

struct ApiResponse {
    bool success;
    std::string message;
    std::string data;  
    double executionTime;
    
    ApiResponse(bool s = false, const std::string& msg = "", const std::string& d = "", double t = 0.0)
        : success(s), message(msg), data(d), executionTime(t) {}
};

struct ArbitrageCheckRequest {
    std::vector<Quote> quotes;
    MarketData marketData;
    std::string interpolationMethod = "bilinear"; 
    bool enableQPCorrection = true;

    QPSolver::Config qpConfig;

    ArbitrageDetector::Config arbConfig;
};

struct ArbitrageCheckResponse {
    bool arbitrageFree;
    double qualityScore;
    std::vector<ArbViolation> violations;
    QPResult qpResult;
    double detectionTime;
    double correctionTime;
    std::string surfaceType;

    std::string toJson() const;
};

class CachedVolSurface {
public:
    CachedVolSurface(const std::vector<Quote>& quotes, const MarketData& marketData);

    double impliedVol(double strike, double expiry) const;

    void clearCache();
    size_t getCacheSize() const;
    void setCacheSize(size_t maxSize);

    double getCacheHitRate() const;
    void resetPerformanceMetrics();

private:
    mutable std::mutex cacheMutex_;
    std::unique_ptr<VolSurface> surface_;
    
    struct CacheKey {
        double strike;
        double expiry;
        
        bool operator<(const CacheKey& other) const {
            if (std::abs(strike - other.strike) > 1e-10) return strike < other.strike;
            return expiry < other.expiry;
        }
    };
    
    mutable std::map<CacheKey, double> cache_;
    mutable size_t cacheHits_ = 0;
    mutable size_t cacheMisses_ = 0;
    size_t maxCacheSize_ = 10000;
};

class ThreadSafeArbitrageDetector {
public:
    explicit ThreadSafeArbitrageDetector(const VolSurface& surface);

    std::vector<ArbViolation> detect() const;

    std::vector<std::vector<ArbViolation>> detectBatch(const std::vector<VolSurface>& surfaces) const;

private:
    mutable std::mutex detectorMutex_;
    std::unique_ptr<ArbitrageDetector> detector_;
};

class VolatilityArbitrageAPI {
public:
    static VolatilityArbitrageAPI& getInstance();

    ApiResponse checkArbitrage(const ArbitrageCheckRequest& request);
    ApiResponse correctSurface(const ArbitrageCheckRequest& request);
    ApiResponse analyzeQuality(const ArbitrageCheckRequest& request);

    ApiResponse batchCheckArbitrage(const std::vector<ArbitrageCheckRequest>& requests);

    void startRealTimeProcessing(std::function<void(const ArbitrageCheckRequest&)> callback);
    void stopRealTimeProcessing();
    bool enqueueRealTimeRequest(const ArbitrageCheckRequest& request);

    ApiResponse updateConfiguration(const std::string& configJson);
    ApiResponse getConfiguration();

    ApiResponse getStatus();
    ApiResponse getPerformanceMetrics();

    ApiResponse loadData(const std::string& dataSource);
    ApiResponse exportData(const std::string& format, const std::string& filePath);

    bool healthCheck();
    std::string getVersion();

private:
    VolatilityArbitrageAPI() = default;

    ArbitrageCheckResponse processRequestInternal(const ArbitrageCheckRequest& request);
    ArbitrageCheckResponse processWithSVI(const ArbitrageCheckRequest& request);
    ArbitrageCheckResponse processWithBilinear(const ArbitrageCheckRequest& request);
    ArbitrageCheckResponse processRawOrderbook(const ArbitrageCheckRequest& request);

    struct PerformanceMetrics {
        double totalProcessingTime = 0.0;
        int totalRequests = 0;
        int successfulRequests = 0;
        double averageResponseTime = 0.0;
        std::map<std::string, double> operationTimes;
    };
    
    mutable std::mutex metricsMutex_;
    PerformanceMetrics metrics_;

    std::thread processingThread_;
    std::atomic<bool> processingActive_{false};
    arena::SPSCBuffer<ArbitrageCheckRequest> requestQueue_{1024};

    mutable std::mutex configMutex_;
    ArbitrageDetector::Config defaultArbConfig_;
    QPSolver::Config defaultQPConfig_;
    QPSolver qpSolver_;
};

class RestAPIHandler {
public:
    
    std::string handlePostArbitrageCheck(const std::string& requestBody);
    std::string handleGetStatus();
    std::string handleGetConfig();
    std::string handlePostConfig(const std::string& requestBody);

    std::string createErrorResponse(const std::string& error, int httpCode = 400);
    std::string createSuccessResponse(const std::string& data);

    ArbitrageCheckRequest parseArbitrageRequest(const std::string& json);
    std::string serializeResponse(const ApiResponse& response);

private:
    VolatilityArbitrageAPI& api_ = VolatilityArbitrageAPI::getInstance();
};

class PerformanceOptimizer {
public:
    
    template<typename Func, typename... Args>
    static auto parallelProcess(Func func, Args&&... args) -> std::vector<decltype(func(args...))>;

    template<typename T>
    class ObjectPool {
    public:
        ObjectPool(size_t initialSize = 100);
        std::unique_ptr<T> acquire();
        void release(std::unique_ptr<T> obj);
        
    private:
        std::vector<std::unique_ptr<T>> pool_;
        std::mutex poolMutex_;
    };

    static void vectorizedBlackScholes(const double* S, const double* K, const double* T, 
                                      const double* sigma, const double* r, const double* q,
                                      double* prices, size_t n);
};

class AsyncTaskProcessor {
public:
    template<typename Func, typename... Args>
    static std::future<typename std::result_of<Func(Args...)>::type> 
    submitAsync(Func&& func, Args&&... args);
    
    void processBatch(const std::vector<std::function<void()>>& tasks);
};
