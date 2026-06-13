

#pragma once
#include "vol_surface.hpp"
#include <vector>
#include <string>
#include <memory>
#include <map>

enum class DataSource {
    JSON_FILE,   
    CSV_FILE,    
    BLOOMBERG,   
    REUTERS,     
    CUSTOM_API   
};

struct DataQualityMetrics {
    int totalQuotes = 0;         
    int validQuotes = 0;         
    int duplicateQuotes = 0;     
    int rejectedQuotes = 0;      
    int outlierQuotes = 0;       
    int missingQuotes = 0;       
    double completenessRatio = 0.0; 
    double consistencyScore = 0.0;  
    std::vector<std::string> validationErrors; 

    double getOverallQuality() const;

    bool isAcceptable() const;
};

class DataHandler {
public:
    
    struct Config {
        DataSource source = DataSource::JSON_FILE;
        std::string filePath;                    
        std::map<std::string, std::string> apiCredentials; 
        double outlierThreshold = 3.0;           
        double minVol = 0.01;                    
        double maxVol = 3.0;                     
        double minTimeToExpiry = 0.001;          
        double maxTimeToExpiry = 10.0;           
        bool enableDuplicateRemoval = true;      
        bool enableOutlierDetection = true;      
        bool enableDataCleaning = true;          
        bool requireBidAsk = false;              
        double minSpread = 0.001;                
    };

    explicit DataHandler(const Config& config = Config{});

    std::pair<std::vector<Quote>, MarketData> loadData();

    bool validateQuote(const Quote& quote, std::string& errorMessage) const;

    std::vector<Quote> cleanData(const std::vector<Quote>& rawQuotes) const;

    const DataQualityMetrics& getQualityMetrics() const { return qualityMetrics_; }

    bool exportData(const std::vector<Quote>& quotes, const std::string& filePath) const;

private:
    Config config_;
    mutable DataQualityMetrics qualityMetrics_;

    std::pair<std::vector<Quote>, MarketData> loadFromJSON() const;
    std::pair<std::vector<Quote>, MarketData> loadFromCSV() const;
    std::pair<std::vector<Quote>, MarketData> loadFromAPI() const;

    bool validateMarketData(const MarketData& marketData, std::string& errorMessage) const;
    void detectOutliers(std::vector<Quote>& quotes) const;
    void removeDuplicates(std::vector<Quote>& quotes) const;
    void fillMissingData(std::vector<Quote>& quotes) const;

    void calculateQualityMetrics(const std::vector<Quote>& original, 
                                 const std::vector<Quote>& cleaned);

    double calculateZScore(double value, double mean, double stdDev) const;
    bool isOutlier(double value, double mean, double stdDev) const;
    std::vector<double> calculateVolatilityStats(const std::vector<Quote>& quotes) const;
};

class DataFeed {
public:
    virtual ~DataFeed() = default;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual std::vector<Quote> getLatestQuotes() = 0;
    virtual MarketData getLatestMarketData() = 0;
    virtual bool isConnected() const = 0;
    virtual std::string getStatus() const = 0;
};

class BloombergFeed : public DataFeed {
public:
    explicit BloombergFeed(const std::string& ticker);
    bool connect() override;
    bool disconnect() override;
    std::vector<Quote> getLatestQuotes() override;
    MarketData getLatestMarketData() override;
    bool isConnected() const override;
    std::string getStatus() const override;

private:
    std::string ticker_;
    bool connected_ = false;
};

class CSVFeed : public DataFeed {
public:
    explicit CSVFeed(const std::string& filePath);
    bool connect() override;
    bool disconnect() override;
    std::vector<Quote> getLatestQuotes() override;
    MarketData getLatestMarketData() override;
    bool isConnected() const override;
    std::string getStatus() const override;

private:
    std::string filePath_;
    bool connected_ = false;
    std::vector<Quote> cachedQuotes_;
    MarketData cachedMarketData_;
};
