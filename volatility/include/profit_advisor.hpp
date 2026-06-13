

#pragma once
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

struct TradeLeg {
    enum class Action { BUY, SELL };
    enum class OptionType { CALL, PUT };

    Action action;
    OptionType optionType;
    double strike;
    double expiry;
    double quantity;       
    double price;       

    nlohmann::json toJson() const;
};

struct TradeRecommendation {
    std::string name;               
    std::string strategy;           
    std::string description;        
    std::vector<TradeLeg> legs;     
    
    double expectedProfit;          
    double maxRisk;                 
    double netCost;                 
    double profitRatio;             
    
    double severity;                
    std::string urgency;            

    ArbType violationType;
    double violationMagnitude;

    nlohmann::json toJson() const;
};

class ProfitAdvisor {
public:
    
    ProfitAdvisor(const VolSurface& surface,
                  const std::vector<ArbViolation>& violations);

    std::vector<TradeRecommendation> generateRecommendations() const;

    static void printReport(const std::vector<TradeRecommendation>& trades);

    static nlohmann::json toJson(const std::vector<TradeRecommendation>& trades);

private:
    const VolSurface& surface_;
    const std::vector<ArbViolation>& violations_;

    std::vector<TradeRecommendation> generateButterflyTrades() const;
    std::vector<TradeRecommendation> generateCalendarTrades() const;
    std::vector<TradeRecommendation> generateVerticalTrades() const;

    double callPrice(double K, double T) const;
    double putPrice(double K, double T) const;

    bool findAdjacentStrikes(double targetK, double& K_lower, double& K_upper) const;

    bool findNextExpiry(double targetT, double& T_next) const;

    static std::string classifyUrgency(double severity);
};
