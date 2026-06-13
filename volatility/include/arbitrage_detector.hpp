

#pragma once
#include "vol_surface.hpp"
#include <vector>
#include <string>

enum class ArbType {
    ButterflyViolation,      
    CalendarViolation,       
    MonotonicityViolation,   
    VerticalSpreadViolation, 
    TimeSpreadValueViolation,
    DensityIntegrityViolation, 
    ExtremeValueViolation    
};

struct ArbViolation {
    ArbType type;           
    double strike;          
    double expiry;          
    double magnitude;       
    double threshold;       
    std::string description; 

    double severityScore() const;

    bool isCritical() const;
};

class ArbitrageDetector {
public:
    
    struct Config {
        double butterflyThreshold = 1e-6;     
        double calendarThreshold = 1e-6;      
        double monotonicityThreshold = 1e-6;  
        double verticalSpreadThreshold = 1e-6;
        double extremeValueThreshold = 10.0;  
        double densityIntegrityThreshold = 1e-3; 
        bool enableDensityCheck = true;       
        bool enableExtremeValueCheck = true;  

        bool enableParallelization = true;    
        int numThreads = 0;                   
        int minWorkPerThread = 100;           
    };

    explicit ArbitrageDetector(const VolSurface& surface);

    void setConfig(const Config& config) { config_ = config; }

    std::vector<ArbViolation> detect() const;

    std::vector<ArbViolation> checkButterfly() const;

    std::vector<ArbViolation> checkCalendar() const;

    std::vector<ArbViolation> checkMonotonicity() const;

    std::vector<ArbViolation> checkVerticalSpread() const;

    std::vector<ArbViolation> checkExtremeValues() const;

    std::vector<ArbViolation> checkDensityIntegrity() const;

    static void report(const std::vector<ArbViolation>& violations);

    double getQualityScore() const;

private:
    const VolSurface& surface_;
    Config config_;

    double d2CdK2(double K, double T, double dK) const;
    double dCdT(double K, double T, double dT) const;
    double dCdK(double K, double T, double dK) const;

    struct AdaptiveStepConfig {
        double h_min_K = 1e-6;      
        double h_max_K = 1e-2;      
        double h_min_T = 1e-8;      
        double h_max_T = 1e-3;      
        double safety_factor = 0.8; 
    };
    
    AdaptiveStepConfig adaptiveConfig_;

    double adaptiveStepSize(double K, double T) const;
    double computeOptimalStepK(double K, double T) const;
    double computeOptimalStepT(double K, double T) const;
    double computeOptimalStepK_2nd(double K, double T) const;

    double estimate3rdDerivativeK(double K, double T) const;
    double estimate3rdDerivativeT(double K, double T) const;

    double estimate4thDerivativeK(double K, double T) const;

    bool checkPutCallParity(double K, double T, double tolerance = 1e-6) const;

    double integrateDensity(double T) const;

    double calculateButterflySeverity(double density) const;
    double calculateCalendarSeverity(double timeDecay) const;
};
