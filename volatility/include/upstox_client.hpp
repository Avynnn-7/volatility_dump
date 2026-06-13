

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "vol_surface.hpp"

namespace upstox {

struct Config {
    std::string apiKey;
    std::string apiSecret;
    std::string redirectUri;
    std::string accessToken;
};

class ApiException : public std::runtime_error {
public:
    explicit ApiException(const std::string& msg) : std::runtime_error(msg) {}
};

class Client {
public:
    explicit Client(const Config& config);

    std::string getInstrumentKey(const std::string& symbol, const std::string& exchange = "NSE_EQ") const;

    std::pair<std::vector<Quote>, MarketData> fetchOptionChain(const std::string& instrumentKey, 
                                                               const std::string& expiry = "") const;

private:
    Config config_;
    
    std::string get(const std::string& path) const;

    static const std::map<std::string, std::string> knownInstruments_;
};

} 

