#include "raw_arbitrage_scanner.hpp"
#include <cmath>
#include <algorithm>
#include <map>
#include <iostream>
#include <sstream>
#include <iomanip>

RawArbitrageScanner::RawArbitrageScanner(const Config& config)
    : config_(config) {}

RawArbitrageScanner::ScanResult RawArbitrageScanner::scan(
    const std::vector<Quote>& quotes, const MarketData& md) const
{
    ScanResult result;
    
    // Filter quotes with valid bids/asks/volumes
    std::vector<Quote> validQuotes;
    for (const auto& q : quotes) {
        if (q.bid > 0 && q.ask > 0 && q.ask >= q.bid && q.strike > 0 && q.expiry > 0) {
            validQuotes.push_back(q);
        }
    }

    if (validQuotes.empty()) {
        return result;
    }

    // Extract unique sorted expiries
    std::vector<double> expiries;
    for (const auto& q : validQuotes) {
        expiries.push_back(q.expiry);
    }
    std::sort(expiries.begin(), expiries.end());
    expiries.erase(std::unique(expiries.begin(), expiries.end()), expiries.end());

    double spot = md.spot;

    // Scan individual components
    scanVerticalSpreads(validQuotes, spot, expiries, result.violations, result.recommendations);
    scanButterflySpreads(validQuotes, spot, expiries, result.violations, result.recommendations);
    scanCalendarSpreads(validQuotes, spot, expiries, result.violations, result.recommendations);
    scanPutCallParity(validQuotes, spot, expiries, result.violations, result.recommendations);
    scanBoxSpreads(validQuotes, spot, expiries, result.violations, result.recommendations);

    // Sort recommendations by expected profit descending
    std::sort(result.recommendations.begin(), result.recommendations.end(),
              [](const TradeRecommendation& a, const TradeRecommendation& b) {
                  return a.expectedProfit > b.expectedProfit;
              });

    return result;
}

void RawArbitrageScanner::scanVerticalSpreads(
    const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries,
    std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const
{
    // For each expiry
    for (double T : expiries) {
        double df = std::exp(-config_.r * T);

        // Group quotes by optionType
        std::vector<Quote> calls;
        std::vector<Quote> puts;
        for (const auto& q : quotes) {
            if (std::abs(q.expiry - T) < 1e-6) {
                if (q.optionType == QuoteOptionType::CALL) {
                    calls.push_back(q);
                } else {
                    puts.push_back(q);
                }
            }
        }

        // Sort by strike
        auto sortByStrike = [](const Quote& a, const Quote& b) { return a.strike < b.strike; };
        std::sort(calls.begin(), calls.end(), sortByStrike);
        std::sort(puts.begin(), puts.end(), sortByStrike);

        // 1. Call Vertical spreads
        for (size_t i = 0; i < calls.size(); ++i) {
            for (size_t j = i + 1; j < calls.size(); ++j) {
                const auto& c1 = calls[i]; // lower strike K1
                const auto& c2 = calls[j]; // higher strike K2
                double K1 = c1.strike;
                double K2 = c2.strike;

                // Case A: Call Monotonicity (Higher strike call priced higher than lower strike call)
                // C(K2, Bid) > C(K1, Ask)
                if (c2.bid > c1.ask + config_.costs) {
                    double profit = c2.bid - c1.ask;
                    
                    ArbViolation viol;
                    viol.type = ArbType::MonotonicityViolation;
                    viol.strike = K2;
                    viol.expiry = T;
                    viol.magnitude = profit;
                    viol.threshold = 0.0;
                    viol.description = "Monotonicity Call: C(" + std::to_string(K2) + ", Bid)=" + std::to_string(c2.bid) +
                                       " > C(" + std::to_string(K1) + ", Ask)=" + std::to_string(c1.ask);
                    violations.push_back(viol);

                    TradeRecommendation rec;
                    rec.strategy = "VERTICAL";
                    rec.name = "Call Monotonicity Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + " T=" + std::to_string(T);
                    rec.description = "The higher strike call at " + std::to_string(int(K2)) + " is priced higher than the lower strike " + std::to_string(int(K1)) + ". Buy K1, sell K2 for a risk-free credit.";
                    rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K1, T, 1.0, c1.ask});
                    rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::CALL, K2, T, 1.0, c2.bid});
                    rec.expectedProfit = profit;
                    rec.maxRisk = 0.0;
                    rec.netCost = -profit;
                    rec.profitRatio = 9999.0;
                    rec.severity = 1.0;
                    rec.urgency = "HIGH";
                    rec.violationType = viol.type;
                    rec.violationMagnitude = viol.magnitude;
                    recommendations.push_back(rec);
                }

                // Case B: Vertical Spread upper boundary violation
                // C(K1, Bid) - C(K2, Ask) > (K2 - K1) * df
                double maxVal = (K2 - K1) * df;
                if (c1.bid - c2.ask > maxVal + config_.costs) {
                    double profit = (c1.bid - c2.ask) - maxVal;

                    ArbViolation viol;
                    viol.type = ArbType::VerticalSpreadViolation;
                    viol.strike = K1;
                    viol.expiry = T;
                    viol.magnitude = profit;
                    viol.threshold = maxVal;
                    viol.description = "Vertical Call Spread: C(" + std::to_string(K1) + ", Bid) - C(" + std::to_string(K2) + ", Ask)=" + 
                                       std::to_string(c1.bid - c2.ask) + " > maximum value " + std::to_string(maxVal);
                    violations.push_back(viol);

                    TradeRecommendation rec;
                    rec.strategy = "VERTICAL";
                    rec.name = "Call Vertical Spread Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + " T=" + std::to_string(T);
                    rec.description = "The spread premium collected by selling the " + std::to_string(int(K1)) + " call and buying the " + std::to_string(int(K2)) + " call exceeds the maximum possible risk-neutral payoff.";
                    rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::CALL, K1, T, 1.0, c1.bid});
                    rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K2, T, 1.0, c2.ask});
                    rec.expectedProfit = profit;
                    rec.maxRisk = 0.0;
                    rec.netCost = -(c1.bid - c2.ask);
                    rec.profitRatio = 9999.0;
                    rec.severity = 1.0;
                    rec.urgency = "HIGH";
                    rec.violationType = viol.type;
                    rec.violationMagnitude = viol.magnitude;
                    recommendations.push_back(rec);
                }
            }
        }

        // 2. Put Vertical spreads
        for (size_t i = 0; i < puts.size(); ++i) {
            for (size_t j = i + 1; j < puts.size(); ++j) {
                const auto& p1 = puts[i]; // lower strike K1
                const auto& p2 = puts[j]; // higher strike K2
                double K1 = p1.strike;
                double K2 = p2.strike;

                // Case A: Put Monotonicity (Lower strike put priced higher than higher strike put)
                // P(K1, Bid) > P(K2, Ask)
                if (p1.bid > p2.ask + config_.costs) {
                    double profit = p1.bid - p2.ask;

                    ArbViolation viol;
                    viol.type = ArbType::MonotonicityViolation;
                    viol.strike = K1;
                    viol.expiry = T;
                    viol.magnitude = profit;
                    viol.threshold = 0.0;
                    viol.description = "Monotonicity Put: P(" + std::to_string(K1) + ", Bid)=" + std::to_string(p1.bid) +
                                       " > P(" + std::to_string(K2) + ", Ask)=" + std::to_string(p2.ask);
                    violations.push_back(viol);

                    TradeRecommendation rec;
                    rec.strategy = "VERTICAL";
                    rec.name = "Put Monotonicity Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + " T=" + std::to_string(T);
                    rec.description = "The lower strike put at " + std::to_string(int(K1)) + " is priced higher than the higher strike " + std::to_string(int(K2)) + ". Buy K2, sell K1 for a risk-free credit.";
                    rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K2, T, 1.0, p2.ask});
                    rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::PUT, K1, T, 1.0, p1.bid});
                    rec.expectedProfit = profit;
                    rec.maxRisk = 0.0;
                    rec.netCost = -profit;
                    rec.profitRatio = 9999.0;
                    rec.severity = 1.0;
                    rec.urgency = "HIGH";
                    rec.violationType = viol.type;
                    rec.violationMagnitude = viol.magnitude;
                    recommendations.push_back(rec);
                }

                // Case B: Vertical Spread upper boundary violation
                // P(K2, Bid) - P(K1, Ask) > (K2 - K1) * df
                double maxVal = (K2 - K1) * df;
                if (p2.bid - p1.ask > maxVal + config_.costs) {
                    double profit = (p2.bid - p1.ask) - maxVal;

                    ArbViolation viol;
                    viol.type = ArbType::VerticalSpreadViolation;
                    viol.strike = K2;
                    viol.expiry = T;
                    viol.magnitude = profit;
                    viol.threshold = maxVal;
                    viol.description = "Vertical Put Spread: P(" + std::to_string(K2) + ", Bid) - P(" + std::to_string(K1) + ", Ask)=" + 
                                       std::to_string(p2.bid - p1.ask) + " > maximum value " + std::to_string(maxVal);
                    violations.push_back(viol);

                    TradeRecommendation rec;
                    rec.strategy = "VERTICAL";
                    rec.name = "Put Vertical Spread Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + " T=" + std::to_string(T);
                    rec.description = "The spread premium collected by selling the " + std::to_string(int(K2)) + " put and buying the " + std::to_string(int(K1)) + " put exceeds the maximum possible risk-neutral payoff.";
                    rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::PUT, K2, T, 1.0, p2.bid});
                    rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K1, T, 1.0, p1.ask});
                    rec.expectedProfit = profit;
                    rec.maxRisk = 0.0;
                    rec.netCost = -(p2.bid - p1.ask);
                    rec.profitRatio = 9999.0;
                    rec.severity = 1.0;
                    rec.urgency = "HIGH";
                    rec.violationType = viol.type;
                    rec.violationMagnitude = viol.magnitude;
                    recommendations.push_back(rec);
                }
            }
        }
    }
}

void RawArbitrageScanner::scanButterflySpreads(
    const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries,
    std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const
{
    for (double T : expiries) {
        // Group calls and puts
        std::vector<Quote> calls;
        std::vector<Quote> puts;
        for (const auto& q : quotes) {
            if (std::abs(q.expiry - T) < 1e-6) {
                if (q.optionType == QuoteOptionType::CALL) {
                    calls.push_back(q);
                } else {
                    puts.push_back(q);
                }
            }
        }

        auto sortByStrike = [](const Quote& a, const Quote& b) { return a.strike < b.strike; };
        std::sort(calls.begin(), calls.end(), sortByStrike);
        std::sort(puts.begin(), puts.end(), sortByStrike);

        // 1. Call Butterfly
        // For any 3 strikes K1 < K2 < K3
        // We scan adjacent and near-adjacent combinations (strike index difference <= 3) to keep it fast
        int nC = static_cast<int>(calls.size());
        for (int i = 0; i < nC; ++i) {
            for (int j = i + 1; j < std::min(nC, i + 4); ++j) {
                for (int k = j + 1; k < std::min(nC, j + 4); ++k) {
                    const auto& c1 = calls[i];
                    const auto& c2 = calls[j];
                    const auto& c3 = calls[k];

                    double K1 = c1.strike;
                    double K2 = c2.strike;
                    double K3 = c3.strike;

                    // Weights for general butterfly:
                    // w1 = K3 - K2, w2 = K3 - K1, w3 = K2 - K1
                    double w1 = K3 - K2;
                    double w2 = K3 - K1;
                    double w3 = K2 - K1;

                    // Cost of buying the butterfly = w1 * Ask(K1) - w2 * Bid(K2) + w3 * Ask(K3)
                    double cost = w1 * c1.ask - w2 * c2.bid + w3 * c3.ask;

                    // If cost is negative, it's a net credit -> pure arbitrage!
                    if (cost < -config_.costs * w2) {
                        double profit = -cost;
                        double maxRisk = std::max(0.0, cost); // risk-free since cost is negative

                        ArbViolation viol;
                        viol.type = ArbType::ButterflyViolation;
                        viol.strike = K2;
                        viol.expiry = T;
                        viol.magnitude = cost / w2; // normalized density violation
                        viol.threshold = 0.0;
                        viol.description = "Butterfly Call: K=" + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + "/" + std::to_string(int(K3)) +
                                           " NetCost=" + std::to_string(cost) + " < 0";
                        violations.push_back(viol);

                        TradeRecommendation rec;
                        rec.strategy = "BUTTERFLY";
                        rec.name = "Call Butterfly Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + "/" + std::to_string(int(K3)) + " T=" + std::to_string(T);
                        rec.description = "The butterfly spread can be entered for a net credit, offering a risk-free return with additional upside if spot expires near K2.";
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K1, T, w1, c1.ask});
                        rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::CALL, K2, T, w2, c2.bid});
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K3, T, w3, c3.ask});
                        rec.expectedProfit = profit;
                        rec.maxRisk = maxRisk;
                        rec.netCost = cost;
                        rec.profitRatio = 9999.0;
                        rec.severity = 1.0;
                        rec.urgency = "HIGH";
                        rec.violationType = viol.type;
                        rec.violationMagnitude = viol.magnitude;
                        recommendations.push_back(rec);
                    }
                }
            }
        }

        // 2. Put Butterfly
        int nP = static_cast<int>(puts.size());
        for (int i = 0; i < nP; ++i) {
            for (int j = i + 1; j < std::min(nP, i + 4); ++j) {
                for (int k = j + 1; k < std::min(nP, j + 4); ++k) {
                    const auto& p1 = puts[i];
                    const auto& p2 = puts[j];
                    const auto& p3 = puts[k];

                    double K1 = p1.strike;
                    double K2 = p2.strike;
                    double K3 = p3.strike;

                    double w1 = K3 - K2;
                    double w2 = K3 - K1;
                    double w3 = K2 - K1;

                    // Cost of buying the butterfly = w1 * Ask(K1) - w2 * Bid(K2) + w3 * Ask(K3)
                    double cost = w1 * p1.ask - w2 * p2.bid + w3 * p3.ask;

                    // If cost is negative, it's a net credit -> pure arbitrage!
                    if (cost < -config_.costs * w2) {
                        double profit = -cost;
                        double maxRisk = std::max(0.0, cost);

                        ArbViolation viol;
                        viol.type = ArbType::ButterflyViolation;
                        viol.strike = K2;
                        viol.expiry = T;
                        viol.magnitude = cost / w2;
                        viol.threshold = 0.0;
                        viol.description = "Butterfly Put: K=" + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + "/" + std::to_string(int(K3)) +
                                           " NetCost=" + std::to_string(cost) + " < 0";
                        violations.push_back(viol);

                        TradeRecommendation rec;
                        rec.strategy = "BUTTERFLY";
                        rec.name = "Put Butterfly Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + "/" + std::to_string(int(K3)) + " T=" + std::to_string(T);
                        rec.description = "The butterfly spread can be entered for a net credit, offering a risk-free return with additional upside if spot expires near K2.";
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K1, T, w1, p1.ask});
                        rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::PUT, K2, T, w2, p2.bid});
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K3, T, w3, p3.ask});
                        rec.expectedProfit = profit;
                        rec.maxRisk = maxRisk;
                        rec.netCost = cost;
                        rec.profitRatio = 9999.0;
                        rec.severity = 1.0;
                        rec.urgency = "HIGH";
                        rec.violationType = viol.type;
                        rec.violationMagnitude = viol.magnitude;
                        recommendations.push_back(rec);
                    }
                }
            }
        }
    }
}

void RawArbitrageScanner::scanCalendarSpreads(
    const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries,
    std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const
{
    if (expiries.size() < 2) return;

    // Build map for quick lookup: "strike|expiry|type" -> Quote
    std::map<std::string, Quote> quoteMap;
    for (const auto& q : quotes) {
        std::string key = std::to_string(int(q.strike)) + "|" + std::to_string(q.expiry) + "|" +
                          (q.optionType == QuoteOptionType::CALL ? "C" : "P");
        quoteMap[key] = q;
    }

    // Scan unique strikes
    std::vector<double> strikes;
    for (const auto& q : quotes) {
        strikes.push_back(q.strike);
    }
    std::sort(strikes.begin(), strikes.end());
    strikes.erase(std::unique(strikes.begin(), strikes.end()), strikes.end());

    for (double K : strikes) {
        for (size_t i = 0; i < expiries.size(); ++i) {
            for (size_t j = i + 1; j < expiries.size(); ++j) {
                double T1 = expiries[i]; // near term
                double T2 = expiries[j]; // far term

                // 1. Call Calendar
                std::string k1c = std::to_string(int(K)) + "|" + std::to_string(T1) + "|C";
                std::string k2c = std::to_string(int(K)) + "|" + std::to_string(T2) + "|C";

                if (quoteMap.count(k1c) && quoteMap.count(k2c)) {
                    const auto& q1 = quoteMap[k1c];
                    const auto& q2 = quoteMap[k2c];

                    // Calendar violation if near-term Bid exceeds far-term Ask
                    // C(T1, Bid) > C(T2, Ask)
                    if (q1.bid > q2.ask + config_.costs) {
                        double profit = q1.bid - q2.ask;

                        ArbViolation viol;
                        viol.type = ArbType::CalendarViolation;
                        viol.strike = K;
                        viol.expiry = T1;
                        viol.magnitude = profit;
                        viol.threshold = 0.0;
                        viol.description = "Calendar Call: C(T=" + std::to_string(T1) + ", Bid)=" + std::to_string(q1.bid) +
                                           " > C(T=" + std::to_string(T2) + ", Ask)=" + std::to_string(q2.ask);
                        violations.push_back(viol);

                        TradeRecommendation rec;
                        rec.strategy = "CALENDAR";
                        rec.name = "Call Calendar Arbitrage K=" + std::to_string(int(K)) + " T=" + std::to_string(T1) + "/" + std::to_string(T2);
                        rec.description = "The near-expiry option is priced higher than the far-expiry option at the same strike. Sell near, buy far to capture risk-free credit.";
                        rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::CALL, K, T1, 1.0, q1.bid});
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K, T2, 1.0, q2.ask});
                        rec.expectedProfit = profit;
                        rec.maxRisk = 0.0;
                        rec.netCost = -profit;
                        rec.profitRatio = 9999.0;
                        rec.severity = 1.0;
                        rec.urgency = "HIGH";
                        rec.violationType = viol.type;
                        rec.violationMagnitude = viol.magnitude;
                        recommendations.push_back(rec);
                    }
                }

                // 2. Put Calendar
                std::string k1p = std::to_string(int(K)) + "|" + std::to_string(T1) + "|P";
                std::string k2p = std::to_string(int(K)) + "|" + std::to_string(T2) + "|P";

                if (quoteMap.count(k1p) && quoteMap.count(k2p)) {
                    const auto& q1 = quoteMap[k1p];
                    const auto& q2 = quoteMap[k2p];

                    if (q1.bid > q2.ask + config_.costs) {
                        double profit = q1.bid - q2.ask;

                        ArbViolation viol;
                        viol.type = ArbType::CalendarViolation;
                        viol.strike = K;
                        viol.expiry = T1;
                        viol.magnitude = profit;
                        viol.threshold = 0.0;
                        viol.description = "Calendar Put: P(T=" + std::to_string(T1) + ", Bid)=" + std::to_string(q1.bid) +
                                           " > P(T=" + std::to_string(T2) + ", Ask)=" + std::to_string(q2.ask);
                        violations.push_back(viol);

                        TradeRecommendation rec;
                        rec.strategy = "CALENDAR";
                        rec.name = "Put Calendar Arbitrage K=" + std::to_string(int(K)) + " T=" + std::to_string(T1) + "/" + std::to_string(T2);
                        rec.description = "The near-expiry option is priced higher than the far-expiry option at the same strike. Sell near, buy far to capture risk-free credit.";
                        rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::PUT, K, T1, 1.0, q1.bid});
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K, T2, 1.0, q2.ask});
                        rec.expectedProfit = profit;
                        rec.maxRisk = 0.0;
                        rec.netCost = -profit;
                        rec.profitRatio = 9999.0;
                        rec.severity = 1.0;
                        rec.urgency = "HIGH";
                        rec.violationType = viol.type;
                        rec.violationMagnitude = viol.magnitude;
                        recommendations.push_back(rec);
                    }
                }
            }
        }
    }
}

void RawArbitrageScanner::scanPutCallParity(
    const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries,
    std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const
{
    // Build map: "strike|expiry|type" -> Quote
    std::map<std::string, Quote> quoteMap;
    for (const auto& q : quotes) {
        std::string key = std::to_string(int(q.strike)) + "|" + std::to_string(q.expiry) + "|" +
                          (q.optionType == QuoteOptionType::CALL ? "C" : "P");
        quoteMap[key] = q;
    }

    std::vector<double> strikes;
    for (const auto& q : quotes) {
        strikes.push_back(q.strike);
    }
    std::sort(strikes.begin(), strikes.end());
    strikes.erase(std::unique(strikes.begin(), strikes.end()), strikes.end());

    for (double T : expiries) {
        double df = std::exp(-config_.r * T);

        for (double K : strikes) {
            std::string kc = std::to_string(int(K)) + "|" + std::to_string(T) + "|C";
            std::string kp = std::to_string(int(K)) + "|" + std::to_string(T) + "|P";

            if (quoteMap.count(kc) && quoteMap.count(kp)) {
                const auto& c = quoteMap[kc];
                const auto& p = quoteMap[kp];

                // 1. Conversion Arbitrage: Sell Call (Bid), Buy Put (Ask), Buy Spot (Ask), Borrow K * df
                // Profit = Call_Bid - Put_Ask - Spot_Ask + K * df
                // Spot_Ask can be approximated as spot (assuming narrow stock spread) or spot * 1.0005 for slippage
                double stockAsk = spot; // In real life, get spot ask. Let's assume stock is highly liquid.
                double conversionProfit = c.bid - p.ask - stockAsk + K * df;

                if (conversionProfit > config_.costs) {
                    ArbViolation viol;
                    viol.type = ArbType::VerticalSpreadViolation; // Put call parity is a form of spread boundary
                    viol.strike = K;
                    viol.expiry = T;
                    viol.magnitude = conversionProfit;
                    viol.threshold = 0.0;
                    viol.description = "Conversion Parity: C_Bid(" + std::to_string(c.bid) + ") - P_Ask(" + std::to_string(p.ask) +
                                       ") - Spot(" + std::to_string(spot) + ") + K*df(" + std::to_string(K * df) + ") = " + std::to_string(conversionProfit) + " > 0";
                    violations.push_back(viol);

                    TradeRecommendation rec;
                    rec.strategy = "CONVERSION";
                    rec.name = "Conversion Parity Arbitrage K=" + std::to_string(int(K)) + " T=" + std::to_string(T);
                    rec.description = "Put-Call Parity is violated. Sell call, buy put, buy spot stock, borrow the strike present value for a risk-free lock.";
                    rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::CALL, K, T, 1.0, c.bid});
                    rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K, T, 1.0, p.ask});
                    rec.expectedProfit = conversionProfit;
                    rec.maxRisk = 0.0;
                    rec.netCost = -conversionProfit;
                    rec.profitRatio = 9999.0;
                    rec.severity = 1.0;
                    rec.urgency = "HIGH";
                    rec.violationType = viol.type;
                    rec.violationMagnitude = viol.magnitude;
                    recommendations.push_back(rec);
                }

                // 2. Reversal Arbitrage: Buy Call (Ask), Sell Put (Bid), Short Spot (Bid), Lend K * df
                // Profit = Put_Bid - Call_Ask + Spot_Bid - K * df
                double stockBid = spot;
                double reversalProfit = p.bid - c.ask + stockBid - K * df;

                if (reversalProfit > config_.costs) {
                    ArbViolation viol;
                    viol.type = ArbType::VerticalSpreadViolation;
                    viol.strike = K;
                    viol.expiry = T;
                    viol.magnitude = reversalProfit;
                    viol.threshold = 0.0;
                    viol.description = "Reversal Parity: P_Bid(" + std::to_string(p.bid) + ") - C_Ask(" + std::to_string(c.ask) +
                                       ") + Spot(" + std::to_string(spot) + ") - K*df(" + std::to_string(K * df) + ") = " + std::to_string(reversalProfit) + " > 0";
                    violations.push_back(viol);

                    TradeRecommendation rec;
                    rec.strategy = "REVERSAL";
                    rec.name = "Reversal Parity Arbitrage K=" + std::to_string(int(K)) + " T=" + std::to_string(T);
                    rec.description = "Put-Call Parity is violated. Buy call, sell put, short spot stock, lend the strike present value for a risk-free lock.";
                    rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K, T, 1.0, c.ask});
                    rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::PUT, K, T, 1.0, p.bid});
                    rec.expectedProfit = reversalProfit;
                    rec.maxRisk = 0.0;
                    rec.netCost = -reversalProfit;
                    rec.profitRatio = 9999.0;
                    rec.severity = 1.0;
                    rec.urgency = "HIGH";
                    rec.violationType = viol.type;
                    rec.violationMagnitude = viol.magnitude;
                    recommendations.push_back(rec);
                }
            }
        }
    }
}

void RawArbitrageScanner::scanBoxSpreads(
    const std::vector<Quote>& quotes, double spot, const std::vector<double>& expiries,
    std::vector<ArbViolation>& violations, std::vector<TradeRecommendation>& recommendations) const
{
    // Build map: "strike|expiry|type" -> Quote
    std::map<std::string, Quote> quoteMap;
    for (const auto& q : quotes) {
        std::string key = std::to_string(int(q.strike)) + "|" + std::to_string(q.expiry) + "|" +
                          (q.optionType == QuoteOptionType::CALL ? "C" : "P");
        quoteMap[key] = q;
    }

    // Scan unique strikes
    std::vector<double> strikes;
    for (const auto& q : quotes) {
        strikes.push_back(q.strike);
    }
    std::sort(strikes.begin(), strikes.end());
    strikes.erase(std::unique(strikes.begin(), strikes.end()), strikes.end());

    for (double T : expiries) {
        double df = std::exp(-config_.r * T);

        int nS = static_cast<int>(strikes.size());
        for (int i = 0; i < nS; ++i) {
            for (int j = i + 1; j < std::min(nS, i + 5); ++j) {
                double K1 = strikes[i];
                double K2 = strikes[j];

                std::string k1c = std::to_string(int(K1)) + "|" + std::to_string(T) + "|C";
                std::string k2c = std::to_string(int(K2)) + "|" + std::to_string(T) + "|C";
                std::string k1p = std::to_string(int(K1)) + "|" + std::to_string(T) + "|P";
                std::string k2p = std::to_string(int(K2)) + "|" + std::to_string(T) + "|P";

                if (quoteMap.count(k1c) && quoteMap.count(k2c) && quoteMap.count(k1p) && quoteMap.count(k2p)) {
                    const auto& c1 = quoteMap[k1c];
                    const auto& c2 = quoteMap[k2c];
                    const auto& p1 = quoteMap[k1p];
                    const auto& p2 = quoteMap[k2p];

                    // Box Spread: Buy K1 Call (Ask), Sell K2 Call (Bid), Buy K2 Put (Ask), Sell K1 Put (Bid)
                    // Payoff at expiry is exactly K2 - K1.
                    // Cost = [C(K1, Ask) - C(K2, Bid)] + [P(K2, Ask) - P(K1, Bid)]
                    double cost = (c1.ask - c2.bid) + (p2.ask - p1.bid);
                    double presentValOfPayoff = (K2 - K1) * df;

                    if (cost < presentValOfPayoff - config_.costs) {
                        double profit = presentValOfPayoff - cost;

                        ArbViolation viol;
                        viol.type = ArbType::VerticalSpreadViolation;
                        viol.strike = (K1 + K2) / 2.0;
                        viol.expiry = T;
                        viol.magnitude = profit;
                        viol.threshold = presentValOfPayoff;
                        viol.description = "Box Spread: Cost(" + std::to_string(cost) + ") < PayoffPV(" + std::to_string(presentValOfPayoff) + ")";
                        violations.push_back(viol);

                        TradeRecommendation rec;
                        rec.strategy = "BOX";
                        rec.name = "Box Spread Arbitrage " + std::to_string(int(K1)) + "/" + std::to_string(int(K2)) + " T=" + std::to_string(T);
                        rec.description = "Buy K1/K2 Call Spread and Buy K2/K1 Put Spread. The net entry cost is lower than the discounted value of the guaranteed payoff.";
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::CALL, K1, T, 1.0, c1.ask});
                        rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::CALL, K2, T, 1.0, c2.bid});
                        rec.legs.push_back({TradeLeg::Action::BUY, TradeLeg::OptionType::PUT, K2, T, 1.0, p2.ask});
                        rec.legs.push_back({TradeLeg::Action::SELL, TradeLeg::OptionType::PUT, K1, T, 1.0, p1.bid});
                        rec.expectedProfit = profit;
                        rec.maxRisk = 0.0;
                        rec.netCost = cost;
                        rec.profitRatio = 9999.0;
                        rec.severity = 1.0;
                        rec.urgency = "HIGH";
                        rec.violationType = viol.type;
                        rec.violationMagnitude = viol.magnitude;
                        recommendations.push_back(rec);
                    }
                }
            }
        }
    }
}
