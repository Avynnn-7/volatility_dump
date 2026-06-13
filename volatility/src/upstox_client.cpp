

#include "upstox_client.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

#if __has_include("httplib.h")
    #include "httplib.h"
#else
    #pragma message("WARNING: httplib.h not found. Upstox API integration will fail at runtime.")
    namespace httplib {
        using Headers = std::multimap<std::string, std::string>;
        struct ResultData {
            int status = 0;
            std::string body;
        };
        struct Result {
            ResultData* data = nullptr;
            operator bool() const { return false; } 
            ResultData* operator->() { return data; }
        };
        class Client {
        public:
            Client(const char*) {}
            void set_connection_timeout(int, int) {}
            void set_read_timeout(int, int) {}
            Result Get(const char*, const Headers&) { return Result{}; }
        };
    }
#endif

#include <nlohmann/json.hpp>

namespace upstox {

const std::map<std::string, std::string> Client::knownInstruments_ = {
    
    {"NIFTY",      "NSE_INDEX|Nifty 50"},
    {"BANKNIFTY",  "NSE_INDEX|Nifty Bank"},
    {"FINNIFTY",   "NSE_INDEX|Nifty Fin Service"},
    
    {"RELIANCE",   "NSE_EQ|INE002A01018"},
    {"HDFCBANK",   "NSE_EQ|INE040A01034"},
    {"INFY",       "NSE_EQ|INE009A01021"},
    {"TCS",        "NSE_EQ|INE467B01029"},
    {"SBIN",       "NSE_EQ|INE062A01020"},
    {"ICICIBANK",  "NSE_EQ|INE090A01021"},
    
    {"RELIANCE_BSE", "BSE_EQ|500325"},
    {"HDFCBANK_BSE", "BSE_EQ|500180"}
};

Client::Client(const Config& config) : config_(config) {
    if (config_.accessToken.empty()) {
        throw ApiException("Upstox access token is missing or empty.");
    }
}

std::string Client::getInstrumentKey(const std::string& symbol, const std::string& exchange) const {
    std::string lookup = symbol;
    if (exchange == "BSE_EQ") lookup += "_BSE";
    
    auto it = knownInstruments_.find(lookup);
    if (it != knownInstruments_.end()) {
        return it->second;
    }

    throw ApiException("Unknown symbol: " + symbol + " for exchange: " + exchange + 
                       ". Please add it to known instruments map.");
}

std::string Client::get(const std::string& path) const {
    httplib::Client cli("https://api.upstox.com");
    cli.set_connection_timeout(5, 0); 
    cli.set_read_timeout(10, 0);      

    httplib::Headers headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + config_.accessToken}
    };

    auto res = cli.Get(path.c_str(), headers);
    
    if (!res) {
        throw ApiException("HTTP Request failed: Connection Error to api.upstox.com");
    }
    
    if (res->status != 200) {
        throw ApiException("Upstox API Error (HTTP " + std::to_string(res->status) + "): " + res->body);
    }
    
    return res->body;
}

std::pair<std::vector<Quote>, MarketData> Client::fetchOptionChain(const std::string& instrumentKey, 
                                                                   const std::string& expiry) const {
    std::vector<Quote> quotes;
    MarketData md;

    std::string path = "/v2/option/chain?instrument_key=" + instrumentKey;
    if (!expiry.empty()) {
        path += "&expiry_date=" + expiry;
    }

    std::string responseBody = get(path);
    nlohmann::json j;
    
    try {
        j = nlohmann::json::parse(responseBody);
    } catch (const std::exception& e) {
        throw ApiException("Failed to parse Upstox JSON response: " + std::string(e.what()));
    }

    if (j.value("status", "") != "success") {
        throw ApiException("Upstox returned error status: " + responseBody);
    }

    const auto& dataArr = j["data"];
    if (dataArr.empty()) {
        throw ApiException("Empty option chain data returned from Upstox.");
    }

    double spotPrice = 0.0;
    bool spotSet = false;
    
    auto now = std::chrono::system_clock::now();
    double currentYear = 365.25 * 24 * 3600; 

    for (const auto& item : dataArr) {
        double strike = item.value("strike_price", 0.0);

        std::string expStr = item.value("expiry_date", "");
        double T = 0.25; 
        if (!expStr.empty()) {
            
            std::tm tm = {};
            int y, m, d;
#ifdef _MSC_VER
            if (sscanf_s(expStr.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
#else
            if (sscanf(expStr.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
#endif
                tm.tm_year = y - 1900;
                tm.tm_mon = m - 1;
                tm.tm_mday = d;
                tm.tm_hour = 15; 
                tm.tm_min = 30;
                
                auto expTime = std::chrono::system_clock::from_time_t(mktime(&tm));
                auto diffCount = std::chrono::duration_cast<std::chrono::seconds>(expTime - now).count();
                T = std::max(diffCount / currentYear, 0.0001); 
            }
        }

        if (item.contains("call_options") && !item["call_options"].is_null()) {
            const auto& ce = item["call_options"];

            if (!spotSet && ce.contains("underlying_spot_price")) {
                spotPrice = ce.value("underlying_spot_price", 0.0);
                spotSet = true;
            }

            double iv = 0.0;
            if (ce.contains("market_data") && ce["market_data"].contains("implied_volatility")) {
                iv = ce["market_data"]["implied_volatility"].get<double>() / 100.0; 
            }

            double bid = 0.0, ask = 0.0, vol = 0.0;
            if (ce.contains("market_data")) {
                bid = ce["market_data"].value("bid_price", 0.0);
                ask = ce["market_data"].value("ask_price", 0.0);
                vol = ce["market_data"].value("volume", 0.0);
            }
            
            if (iv > 0.001) {
                quotes.push_back({strike, T, iv, bid, ask, vol});
            }
        }

    }
    
    if (!spotSet || spotPrice <= 0.0) {
        throw ApiException("Could not determine underlying spot price from Upstox response.");
    }
    
    md.spot = spotPrice;
    md.riskFreeRate = 0.065; 
    md.dividendYield = 0.0;
    md.valuationDate = "LIVE";
    md.currency = "INR";
    
    return {quotes, md};
}

} 
