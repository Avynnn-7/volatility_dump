

#pragma once
#include <vector>
#include <string>
#include <Eigen/Dense>
#include <unordered_map>
#include <list>
#include <shared_mutex>

enum class QuoteOptionType { CALL, PUT };

struct Quote {
    double strike;      
    double expiry;      
    double iv;          
    double bid = 0.0;   
    double ask = 0.0;   
    double volume = 0.0; 
    QuoteOptionType optionType = QuoteOptionType::CALL;
};

struct MarketData {
    double spot;            
    double riskFreeRate;    
    double dividendYield;   
    std::string valuationDate; 
    std::string currency;   
};

class VolSurface {
public:
    
    struct CacheStats {
        size_t hits = 0;       
        size_t misses = 0;     
        size_t evictions = 0;  

        double hitRate() const { 
            return (hits + misses > 0) ? 
                   static_cast<double>(hits) / (hits + misses) : 0.0; 
        }
    };

    CacheStats getCacheStats() const;

    void clearCache();

    void setCacheSize(size_t maxEntries);

    explicit VolSurface(const std::vector<Quote>& quotes, const MarketData& marketData);

    double impliedVol(double strike, double expiry) const;

    double callPrice(double strike, double expiry) const;

    double putPrice(double strike, double expiry) const;

    const std::vector<double>& strikes() const { return strikes_; }

    const std::vector<double>& expiries() const { return expiries_; }

    double spot() const { return marketData_.spot; }

    const MarketData& marketData() const { return marketData_; }

    const Eigen::MatrixXd& ivGrid() const { return ivGrid_; }

    double forward(double expiry) const;

    double discountFactor(double expiry) const;

    void print() const;

private:
    MarketData marketData_;          
    std::vector<double> strikes_;    
    std::vector<double> expiries_;   
    Eigen::MatrixXd ivGrid_;         

    static double bsCall(double S, double K, double T, double sigma, double r, double q);
    static double bsPut(double S, double K, double T, double sigma, double r, double q);
    static double normalCDF(double x);
    static double normalPDF(double x);

    struct CacheKey {
        double strike;
        double expiry;
        enum class Type { IV, CallPrice, PutPrice } type;
        
        bool operator==(const CacheKey& other) const {
            return strike == other.strike && 
                   expiry == other.expiry && 
                   type == other.type;
        }
    };
    
    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const {
            size_t h1 = std::hash<double>{}(key.strike);
            size_t h2 = std::hash<double>{}(key.expiry);
            size_t h3 = std::hash<int>{}(static_cast<int>(key.type));
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL) ^ (h3 * 0x517cc1b727220a95ULL);
        }
    };
    
    using CacheList = std::list<std::pair<CacheKey, double>>;
    using CacheMap = std::unordered_map<CacheKey, CacheList::iterator, CacheKeyHash>;
    
    mutable CacheList cacheList_;           
    mutable CacheMap cacheMap_;             
    mutable std::shared_mutex cacheMutex_;  
    mutable CacheStats cacheStats_;         
    size_t maxCacheSize_ = 4096;            

    bool getCached(const CacheKey& key, double& value) const;
    void putCache(const CacheKey& key, double value) const;

    double impliedVolUncached(double strike, double expiry) const;
    double callPriceUncached(double strike, double expiry) const;
    double putPriceUncached(double strike, double expiry) const;
};
