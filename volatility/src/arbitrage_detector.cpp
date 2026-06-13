#include "arbitrage_detector.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

#ifdef VOL_ARB_OPENMP_ENABLED
#include <omp.h>
#endif

double ArbViolation::severityScore() const {
    
    double abs_magnitude = std::abs(magnitude);
    
    switch (type) {
        case ArbType::ButterflyViolation:
            
            return std::min(1.0, abs_magnitude * 1000.0);
        case ArbType::CalendarViolation:
            
            return std::min(1.0, abs_magnitude * 100.0);
        case ArbType::MonotonicityViolation:
            
            return std::min(1.0, abs_magnitude * 10.0);
        case ArbType::VerticalSpreadViolation:
            return std::min(1.0, abs_magnitude * 50.0);
        case ArbType::ExtremeValueViolation:
            
            return std::min(1.0, abs_magnitude / 10.0);
        case ArbType::DensityIntegrityViolation:
            
            return std::min(1.0, abs_magnitude * 100.0);
        default:
            return 0.1;
    }
}

bool ArbViolation::isCritical() const {
    return severityScore() > 0.7;
}

ArbitrageDetector::ArbitrageDetector(const VolSurface& surface)
    : surface_(surface) {}

double ArbitrageDetector::adaptiveStepSize(double K, double T) const {
    
    double dK = K * 1e-4;  
    double dT = T * 1e-3;  

    dK = std::max(dK, 1e-6);
    dT = std::max(dT, 1e-8);
    
    return std::max(dK, dT);
}

double ArbitrageDetector::estimate3rdDerivativeK(double K, double T) const {

    double h = 0.01 * K;  
    if (h < 1e-6) h = 1e-6;

    const auto& Ks = surface_.strikes();
    double K_min = Ks.front();
    double K_max = Ks.back();

    if (K - 2*h < K_min || K + 2*h > K_max) {
        return 100.0;  
    }
    
    double C_m2 = surface_.callPrice(K - 2*h, T);
    double C_m1 = surface_.callPrice(K - h, T);
    double C_p1 = surface_.callPrice(K + h, T);
    double C_p2 = surface_.callPrice(K + 2*h, T);
    
    double f3_approx = (-C_m2 + 2*C_m1 - 2*C_p1 + C_p2) / (2 * h * h * h);
    
    return std::abs(f3_approx) + 1e-10;  
}

double ArbitrageDetector::estimate3rdDerivativeT(double K, double T) const {

    double h = 0.01 * T;
    if (h < 1e-6) h = 1e-6;

    const auto& Ts = surface_.expiries();
    double T_min = Ts.front();
    double T_max = Ts.back();
    
    if (T - 2*h < T_min || T + 2*h > T_max) {
        
        return 100.0;  
    }
    
    double C_m2 = surface_.callPrice(K, T - 2*h);
    double C_m1 = surface_.callPrice(K, T - h);
    double C_p1 = surface_.callPrice(K, T + h);
    double C_p2 = surface_.callPrice(K, T + 2*h);
    
    double f3_approx = (-C_m2 + 2*C_m1 - 2*C_p1 + C_p2) / (2 * h * h * h);
    
    return std::abs(f3_approx) + 1e-10;
}

double ArbitrageDetector::computeOptimalStepK(double K, double T) const {

    double C = surface_.callPrice(K, T);
    double f3 = estimate3rdDerivativeK(K, T);
    
    const double eps_mach = 2.22e-16;  
    double h_opt = std::cbrt(3.0 * eps_mach * std::abs(C) / f3);

    h_opt *= adaptiveConfig_.safety_factor;
    h_opt = std::clamp(h_opt, adaptiveConfig_.h_min_K, adaptiveConfig_.h_max_K);
    
    return h_opt;
}

double ArbitrageDetector::estimate4thDerivativeK(double K, double T) const {
    double h = 0.05 * K;  
    if (h < 1e-6) h = 1e-6;
    
    const auto& Ks = surface_.strikes();
    double K_min = Ks.front();
    double K_max = Ks.back();
    
    if (K - 2*h < K_min || K + 2*h > K_max) {
        return 100.0;  
    }
    
    double C_m2 = surface_.callPrice(K - 2*h, T);
    double C_m1 = surface_.callPrice(K - h, T);
    double C_0  = surface_.callPrice(K, T);
    double C_p1 = surface_.callPrice(K + h, T);
    double C_p2 = surface_.callPrice(K + 2*h, T);
    
    double f4_approx = (C_m2 - 4*C_m1 + 6*C_0 - 4*C_p1 + C_p2) / (h * h * h * h);
    
    return std::abs(f4_approx) + 1e-10;
}

double ArbitrageDetector::computeOptimalStepK_2nd(double K, double T) const {
    double C = surface_.callPrice(K, T);
    double f4 = estimate4thDerivativeK(K, T);
    
    const double eps_mach = 2.22e-16;
    double h_opt = std::pow(48.0 * eps_mach * std::abs(C) / f4, 0.25);
    
    h_opt *= adaptiveConfig_.safety_factor;
    h_opt = std::clamp(h_opt, adaptiveConfig_.h_min_K, adaptiveConfig_.h_max_K);
    
    return h_opt;
}

double ArbitrageDetector::computeOptimalStepT(double K, double T) const {

    double C = surface_.callPrice(K, T);
    double f3 = estimate3rdDerivativeT(K, T);
    
    const double eps_mach = 2.22e-16;
    double h_opt = std::cbrt(3.0 * eps_mach * std::abs(C) / f3);
    
    h_opt *= adaptiveConfig_.safety_factor;
    h_opt = std::clamp(h_opt, adaptiveConfig_.h_min_T, adaptiveConfig_.h_max_T);
    
    return h_opt;
}

double ArbitrageDetector::d2CdK2(double K, double T, double dK) const {
    
    double step = (dK > 0) ? dK : computeOptimalStepK_2nd(K, T);
    
    double Cu = surface_.callPrice(K + step, T);
    double Cm = surface_.callPrice(K, T);
    double Cd = surface_.callPrice(K - step, T);
    return (Cu - 2.0 * Cm + Cd) / (step * step);
}

double ArbitrageDetector::dCdT(double K, double T, double dT) const {
    
    double step = (dT > 0) ? dT : computeOptimalStepT(K, T);

    const auto& Ts = surface_.expiries();
    double T_min = Ts.front();
    double T_max = Ts.back();
    double T_clamped = std::clamp(T, T_min, T_max);
    
    double Tlo = T_clamped - step;
    double Thi = T_clamped + step;

    if (Tlo < T_min) {
        double C0 = surface_.callPrice(K, T_clamped);
        double C1 = surface_.callPrice(K, T_clamped + step);
        return (C1 - C0) / step;
    }
    if (Thi > T_max) {
        double C0 = surface_.callPrice(K, T_clamped);
        double C1 = surface_.callPrice(K, T_clamped - step);
        return (C0 - C1) / step;
    }

    double Clo = surface_.callPrice(K, Tlo);
    double Chi = surface_.callPrice(K, Thi);
    return (Chi - Clo) / (2.0 * step);
}

double ArbitrageDetector::dCdK(double K, double T, double dK) const {
    
    double step = (dK > 0) ? dK : computeOptimalStepK(K, T);
    
    double Cu = surface_.callPrice(K + step, T);
    double Cd = surface_.callPrice(K - step, T);
    return (Cu - Cd) / (2.0 * step);
}

std::vector<ArbViolation> ArbitrageDetector::checkButterfly() const {
    const auto& Ks = surface_.strikes();
    const auto& Ts = surface_.expiries();
    int nK = static_cast<int>(Ks.size());
    int nT = static_cast<int>(Ts.size());

    int totalWork = nK * nT;
    bool useParallel = config_.enableParallelization && 
                       totalWork >= config_.minWorkPerThread;

    std::vector<std::vector<ArbViolation>> threadViolations;
    
#ifdef VOL_ARB_OPENMP_ENABLED
    if (useParallel) {
        if (config_.numThreads > 0) {
            omp_set_num_threads(config_.numThreads);
        }
        int maxThreads = omp_get_max_threads();
        threadViolations.resize(maxThreads);
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            std::vector<ArbViolation>& localViols = threadViolations[tid];
            
            #pragma omp for schedule(dynamic, 1) nowait
            for (int i = 0; i < nT; ++i) {
                double T = Ts[i];
                
                for (int j = 1; j + 1 < nK; ++j) {
                    double dK = (Ks[j + 1] - Ks[j - 1]) / 2.0;
                    double d2C = d2CdK2(Ks[j], T, dK);
                    // Breeden-Litzenberger: f(K) = e^{rT} * d²C/dK²
                    double density = surface_.discountFactor(T) > 0 
                        ? d2C / surface_.discountFactor(T) : d2C;
                    
                    if (density < config_.butterflyThreshold) {
                        ArbViolation viol;
                        viol.type = ArbType::ButterflyViolation;
                        viol.strike = Ks[j];
                        viol.expiry = T;
                        viol.magnitude = density;
                        viol.threshold = config_.butterflyThreshold;
                        viol.description = "Butterfly: density = " + std::to_string(density) +
                            " < " + std::to_string(config_.butterflyThreshold) +
                            " at K=" + std::to_string(Ks[j]) + ", T=" + std::to_string(T);
                        localViols.push_back(viol);
                    }
                }
            }
        }
    } else
#endif
    {
        
        threadViolations.resize(1);
        std::vector<ArbViolation>& viols = threadViolations[0];
        
        for (int i = 0; i < nT; ++i) {
            double T = Ts[i];
            for (int j = 1; j + 1 < nK; ++j) {
                double dK = (Ks[j + 1] - Ks[j - 1]) / 2.0;
                double d2C = d2CdK2(Ks[j], T, dK);
                // Breeden-Litzenberger: f(K) = e^{rT} * d²C/dK²
                double density = surface_.discountFactor(T) > 0 
                    ? d2C / surface_.discountFactor(T) : d2C;
                
                if (density < config_.butterflyThreshold) {
                    ArbViolation viol;
                    viol.type = ArbType::ButterflyViolation;
                    viol.strike = Ks[j];
                    viol.expiry = T;
                    viol.magnitude = density;
                    viol.threshold = config_.butterflyThreshold;
                    viol.description = "Butterfly: density = " + std::to_string(density) +
                        " < " + std::to_string(config_.butterflyThreshold) +
                        " at K=" + std::to_string(Ks[j]) + ", T=" + std::to_string(T);
                    viols.push_back(viol);
                }
            }
        }
    }

    std::vector<ArbViolation> allViols;
    size_t totalSize = 0;
    for (const auto& tv : threadViolations) totalSize += tv.size();
    allViols.reserve(totalSize);
    for (auto& tv : threadViolations) {
        allViols.insert(allViols.end(),
            std::make_move_iterator(tv.begin()),
            std::make_move_iterator(tv.end()));
    }
    
    return allViols;
}

std::vector<ArbViolation> ArbitrageDetector::checkCalendar() const {
    const auto& Ks = surface_.strikes();
    const auto& Ts = surface_.expiries();
    int nK = static_cast<int>(Ks.size());
    int nT = static_cast<int>(Ts.size());
    
    int totalWork = nK * nT;
    bool useParallel = config_.enableParallelization && 
                       totalWork >= config_.minWorkPerThread;
    
    std::vector<std::vector<ArbViolation>> threadViolations;
    
#ifdef VOL_ARB_OPENMP_ENABLED
    if (useParallel) {
        int maxThreads = omp_get_max_threads();
        threadViolations.resize(maxThreads);
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            std::vector<ArbViolation>& localViols = threadViolations[tid];
            
            #pragma omp for schedule(dynamic, 1) nowait
            for (int j = 0; j < nK; ++j) {
                double K = Ks[j];
                
                for (int i = 0; i + 1 < nT; ++i) {
                    double Teval = (Ts[i] + Ts[i + 1]) / 2.0;
                    
                    double iv1 = surface_.impliedVol(K, Ts[i]);
                    double iv2 = surface_.impliedVol(K, Ts[i + 1]);
                    double w1 = iv1 * iv1 * Ts[i];
                    double w2 = iv2 * iv2 * Ts[i + 1];
                    
                    double dw_dT = (w2 - w1) / (Ts[i+1] - Ts[i]);
                    
                    if (dw_dT < config_.calendarThreshold) {
                        ArbViolation viol;
                        viol.type = ArbType::CalendarViolation;
                        viol.strike = K;
                        viol.expiry = Teval;
                        viol.magnitude = dw_dT;
                        viol.threshold = config_.calendarThreshold;
                        viol.description = "Calendar: dw/dT = " + std::to_string(dw_dT) +
                            " < " + std::to_string(config_.calendarThreshold) +
                            " at K=" + std::to_string(K) + ", T=" + std::to_string(Teval);
                        localViols.push_back(viol);
                    }
                }
            }
        }
    } else
#endif
    {
        
        threadViolations.resize(1);
        std::vector<ArbViolation>& viols = threadViolations[0];
        
        for (int j = 0; j < nK; ++j) {
            double K = Ks[j];
            for (int i = 0; i + 1 < nT; ++i) {
                double Teval = (Ts[i] + Ts[i + 1]) / 2.0;
                
                double iv1 = surface_.impliedVol(K, Ts[i]);
                double iv2 = surface_.impliedVol(K, Ts[i + 1]);
                double w1 = iv1 * iv1 * Ts[i];
                double w2 = iv2 * iv2 * Ts[i + 1];
                
                double dw_dT = (w2 - w1) / (Ts[i+1] - Ts[i]);
                
                if (dw_dT < config_.calendarThreshold) {
                    ArbViolation viol;
                    viol.type = ArbType::CalendarViolation;
                    viol.strike = K;
                    viol.expiry = Teval;
                    viol.magnitude = dw_dT;
                    viol.threshold = config_.calendarThreshold;
                    viol.description = "Calendar: dw/dT = " + std::to_string(dw_dT) +
                        " < " + std::to_string(config_.calendarThreshold) +
                        " at K=" + std::to_string(K) + ", T=" + std::to_string(Teval);
                    viols.push_back(viol);
                }
            }
        }
    }

    std::vector<ArbViolation> allViols;
    size_t totalSize = 0;
    for (const auto& tv : threadViolations) totalSize += tv.size();
    allViols.reserve(totalSize);
    for (auto& tv : threadViolations) {
        allViols.insert(allViols.end(),
            std::make_move_iterator(tv.begin()),
            std::make_move_iterator(tv.end()));
    }
    
    return allViols;
}

std::vector<ArbViolation> ArbitrageDetector::checkMonotonicity() const {
    std::vector<ArbViolation> viols;
    const auto& Ks = surface_.strikes();
    const auto& Ts = surface_.expiries();

    for (double T : Ts) {
        double df = surface_.discountFactor(T);
        for (int j = 1; j + 1 < (int)Ks.size(); ++j) {
            double dK = (Ks[j + 1] - Ks[j - 1]) / 2.0;
            double slope = dCdK(Ks[j], T, dK);
            
            if (slope > config_.monotonicityThreshold) {
                ArbViolation viol;
                viol.type = ArbType::MonotonicityViolation;
                viol.strike = Ks[j];
                viol.expiry = T;
                viol.magnitude = slope;
                viol.threshold = config_.monotonicityThreshold;
                viol.description = "Monotonicity: dC/dK = " + std::to_string(slope) +
                    " > " + std::to_string(config_.monotonicityThreshold) +
                    " at K=" + std::to_string(Ks[j]) + ", T=" + std::to_string(T);
                viols.push_back(viol);
            } else if (slope < -df - config_.monotonicityThreshold) {
                ArbViolation viol;
                viol.type = ArbType::MonotonicityViolation;
                viol.strike = Ks[j];
                viol.expiry = T;
                viol.magnitude = -df - slope;
                viol.threshold = config_.monotonicityThreshold;
                viol.description = "Monotonicity (Digital Put Bound): dC/dK = " + std::to_string(slope) +
                    " < " + std::to_string(-df - config_.monotonicityThreshold) +
                    " at K=" + std::to_string(Ks[j]) + ", T=" + std::to_string(T);
                viols.push_back(viol);
            }
        }
    }
    return viols;
}

std::vector<ArbViolation> ArbitrageDetector::detect() const {
    auto v1 = checkButterfly();
    auto v2 = checkCalendar();
    auto v3 = checkMonotonicity();
    auto v6 = checkVerticalSpread();
    
    // Pre-reserve to avoid reallocation during aggregation
    size_t totalEstimate = v1.size() + v2.size() + v3.size() + v6.size();
    
    if (config_.enableExtremeValueCheck) {
        auto v4 = checkExtremeValues();
        totalEstimate += v4.size();
        v1.reserve(v1.size() + v4.size());
        v1.insert(v1.end(), std::make_move_iterator(v4.begin()),
                             std::make_move_iterator(v4.end()));
    }
    
    if (config_.enableDensityCheck) {
        auto v5 = checkDensityIntegrity();
        totalEstimate += v5.size();
        v1.reserve(v1.size() + v5.size());
        v1.insert(v1.end(), std::make_move_iterator(v5.begin()),
                             std::make_move_iterator(v5.end()));
    }
    
    v1.reserve(v1.size() + v6.size() + v2.size() + v3.size());
    v1.insert(v1.end(), std::make_move_iterator(v6.begin()),
                         std::make_move_iterator(v6.end()));
    v1.insert(v1.end(), std::make_move_iterator(v2.begin()),
                         std::make_move_iterator(v2.end()));
    v1.insert(v1.end(), std::make_move_iterator(v3.begin()),
                         std::make_move_iterator(v3.end()));
    return v1;
}

void ArbitrageDetector::report(const std::vector<ArbViolation>& violations) {
    if (violations.empty()) {
        std::cout << " No arbitrage violations detected.\n";
        return;
    }
    std::cout << "\n=== Arbitrage Violations (" << violations.size() << ") ===\n";

    auto sortedViols = violations;
    std::sort(sortedViols.begin(), sortedViols.end(), 
              [](const ArbViolation& a, const ArbViolation& b) {
                  return a.severityScore() > b.severityScore();
              });
    
    for (const auto& v : sortedViols) {
        std::string type;
        switch (v.type) {
            case ArbType::ButterflyViolation:    type = "[BUTTERFLY]";    break;
            case ArbType::CalendarViolation:     type = "[CALENDAR]";     break;
            case ArbType::MonotonicityViolation: type = "[MONOTONICITY]"; break;
            case ArbType::VerticalSpreadViolation: type = "[VERTICAL]";     break;
            case ArbType::ExtremeValueViolation: type = "[EXTREME]";      break;
            case ArbType::DensityIntegrityViolation: type = "[DENSITY]";   break;
        }
        
        std::string severity = v.isCritical() ? " [CRITICAL]" : "";
        std::cout << std::setw(16) << type << severity << "  " << v.description
                  << "  (severity=" << std::fixed << std::setprecision(3) << v.severityScore() << ")\n";
    }
    std::cout << "\n";
}

std::vector<ArbViolation> ArbitrageDetector::checkExtremeValues() const {
    std::vector<ArbViolation> viols;
    const auto& Ks = surface_.strikes();
    const auto& Ts = surface_.expiries();

    for (double T : Ts) {
        for (double K : Ks) {
            double iv = surface_.impliedVol(K, T);
            
            if (iv > config_.extremeValueThreshold || iv < 0.01) {
                ArbViolation viol;
                viol.type = ArbType::ExtremeValueViolation;
                viol.strike = K;
                viol.expiry = T;
                viol.magnitude = iv;
                viol.threshold = config_.extremeValueThreshold;
                viol.description = "Extreme IV: " + std::to_string(iv * 100) + "% at K=" + 
                    std::to_string(K) + ", T=" + std::to_string(T);
                viols.push_back(viol);
            }
        }
    }
    return viols;
}

std::vector<ArbViolation> ArbitrageDetector::checkVerticalSpread() const {
    std::vector<ArbViolation> viols;
    const auto& Ks = surface_.strikes();
    const auto& Ts = surface_.expiries();

    for (double T : Ts) {
        for (int i = 0; i + 1 < (int)Ks.size(); ++i) {
            double K1 = Ks[i], K2 = Ks[i + 1];
            double C1 = surface_.callPrice(K1, T);
            double C2 = surface_.callPrice(K2, T);

            double spreadValue = C1 - C2;
            double maxAllowed = (K2 - K1) * surface_.discountFactor(T);
            
            if (spreadValue > maxAllowed + config_.verticalSpreadThreshold) {
                ArbViolation viol;
                viol.type = ArbType::VerticalSpreadViolation;
                viol.strike = (K1 + K2) / 2.0;  
                viol.expiry = T;
                viol.magnitude = spreadValue - maxAllowed;
                viol.threshold = config_.verticalSpreadThreshold;
                viol.description = "Vertical spread: C(" + std::to_string(K1) + ") - C(" + 
                    std::to_string(K2) + ") = " + std::to_string(spreadValue) + 
                    " > " + std::to_string(maxAllowed) + " at T=" + std::to_string(T);
                viols.push_back(viol);
            }
        }
    }
    return viols;
}

std::vector<ArbViolation> ArbitrageDetector::checkDensityIntegrity() const {
    std::vector<ArbViolation> viols;
    const auto& Ts = surface_.expiries();

    for (double T : Ts) {
        double integral = integrateDensity(T);
        double error = std::abs(integral - 1.0);
        
        if (error > config_.densityIntegrityThreshold) {
            ArbViolation viol;
            viol.type = ArbType::DensityIntegrityViolation;
            viol.strike = 0.0;  
            viol.expiry = T;
            viol.magnitude = error;
            viol.threshold = config_.densityIntegrityThreshold;
            viol.description = "Density integral: " + std::to_string(integral) + 
                " (error=" + std::to_string(error) + ") at T=" + std::to_string(T);
            viols.push_back(viol);
        }
    }
    return viols;
}

double ArbitrageDetector::integrateDensity(double T) const {
    
    const auto& Ks = surface_.strikes();
    if (Ks.size() < 2) return 1.0;
    
    double r = surface_.marketData().riskFreeRate;
    double discount = std::exp(r * T);
    
    double integral = 0.0;
    for (int i = 0; i + 1 < (int)Ks.size(); ++i) {
        double K1 = Ks[i], K2 = Ks[i + 1];
        double density1 = d2CdK2(K1, T, adaptiveStepSize(K1, T));
        double density2 = d2CdK2(K2, T, adaptiveStepSize(K2, T));

        double q1 = discount * density1;
        double q2 = discount * density2;
        
        integral += 0.5 * (q1 + q2) * (K2 - K1);
    }
    return integral;
}

double ArbitrageDetector::getQualityScore() const {
    auto violations = detect();
    if (violations.empty()) return 1.0;
    
    double totalSeverity = 0.0;
    int criticalCount = 0;
    
    for (const auto& v : violations) {
        totalSeverity += v.severityScore();
        if (v.isCritical()) criticalCount++;
    }

    double criticalPenalty = criticalCount * 0.3;
    double severityPenalty = std::min(0.7, totalSeverity / violations.size());
    
    return std::max(0.0, 1.0 - criticalPenalty - severityPenalty);
}
