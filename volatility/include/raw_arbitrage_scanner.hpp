#pragma once
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "profit_advisor.hpp"
#include <vector>
#include <string>

class RawArbitrageScanner {
public:
    struct Config {
        double r = 0.065;        // Risk-free rate
        double costs = 0.05;     // Transaction/slippage costs per contract
        double min_volume = 1.0; // Minimum volume required to trade a leg
    };

    explicit RawArbitrageScanner(const Config& config = Config());

    struct ScanResult {
        std::vector<ArbViolation> violations;
        std::vector<TradeRecommendation> recommendations;
    };

    ScanResult scan(const std::vector<Quote>& quotes, const MarketData& md) const;

private:
    Config config_;

    void scanVerticalSpreads(const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries, std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const;
    void scanButterflySpreads(const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries, std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const;
    void scanCalendarSpreads(const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries, std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const;
    void scanPutCallParity(const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries, std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const;
    void scanBoxSpreads(const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries, std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const;
};
