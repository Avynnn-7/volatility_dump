#include "svi_surface.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool SVIParams::isValid() const {
    if (b < 0 || rho < -1.0 || rho > 1.0 || sigma < 0) return false;
    double beta_R = b * (1.0 + rho);
    double beta_L = b * (1.0 - rho);
    return (beta_R <= 2.0) && (beta_L <= 2.0);
}

double SVIParams::totalVariance(double logMoneyness) const {
    return a + b * (rho * (logMoneyness - m) + std::sqrt((logMoneyness - m) * (logMoneyness - m) + sigma * sigma));
}

double SVIParams::impliedVol(double logMoneyness, double expiry) const {
    if (expiry <= 0) return 0.0;
    double w = totalVariance(logMoneyness);
    return std::sqrt(w / expiry);
}

SVISurface::SVISurface(const std::vector<Quote>& quotes, const MarketData& marketData)
    : marketData_(marketData)
{

    if (quotes.empty())
        throw std::invalid_argument("SVISurface: no quotes supplied");

    for (const auto& q : quotes) {
        expiries_.push_back(q.expiry);
    }
    std::sort(expiries_.begin(), expiries_.end());
    expiries_.erase(std::unique(expiries_.begin(), expiries_.end()), expiries_.end());

    if (expiries_.size() == 1) {
        std::cerr << "Warning: SVISurface has only one expiry. Interpolation not possible." << std::endl;
    }

    for (double expiry : expiries_) {
        std::vector<Quote> expiryQuotes;
        for (const auto& q : quotes) {
            if (std::abs(q.expiry - expiry) < 1e-6) {
                expiryQuotes.push_back(q);
            }
        }
        
        if (expiryQuotes.empty()) {
            throw std::invalid_argument("SVISurface: no quotes for expiry " + std::to_string(expiry));
        }

        if (expiryQuotes.size() == 1) {
            std::cerr << "Warning: Only 1 quote for expiry " << expiry 
                      << ". Using flat volatility." << std::endl;
        }
        
        SVIParams params = fitSVI(expiryQuotes, expiry);
        sviParams_.push_back(params);
    }
}

double SVISurface::impliedVol(double strike, double expiry) const {

    if (expiries_.empty() || sviParams_.empty()) {
        return 0.20;  
    }

    if (expiries_.size() == 1) {
        double logMoneyness = std::log(strike / forwardPrice(expiries_[0]));
        return sviParams_[0].impliedVol(logMoneyness, expiries_[0]);
    }

    auto it = std::lower_bound(expiries_.begin(), expiries_.end(), expiry);
    
    if (it == expiries_.begin()) {
        
        double logMoneyness = std::log(strike / forwardPrice(expiries_[0]));
        return sviParams_[0].impliedVol(logMoneyness, expiries_[0]);
    }
    
    if (it == expiries_.end()) {
        
        double logMoneyness = std::log(strike / forwardPrice(expiries_.back()));
        return sviParams_.back().impliedVol(logMoneyness, expiries_.back());
    }

    int idx2 = static_cast<int>(it - expiries_.begin());
    int idx1 = idx2 - 1;
    
    double T1 = expiries_[idx1], T2 = expiries_[idx2];
    double weight = (expiry - T1) / (T2 - T1);
    
    double logMoneyness1 = std::log(strike / forwardPrice(T1));
    double logMoneyness2 = std::log(strike / forwardPrice(T2));
    
    double iv1 = sviParams_[idx1].impliedVol(logMoneyness1, T1);
    double iv2 = sviParams_[idx2].impliedVol(logMoneyness2, T2);

    double var1 = iv1 * iv1 * T1;
    double var2 = iv2 * iv2 * T2;
    double var_interp = (1 - weight) * var1 + weight * var2;
    return std::sqrt(var_interp / expiry);
}

bool SVISurface::isArbitrageFree() const {
    return getArbitrageViolations().empty();
}

std::vector<std::string> SVISurface::getArbitrageViolations() const {
    std::vector<std::string> violations;
    
    for (size_t i = 0; i < sviParams_.size(); ++i) {
        const auto& params = sviParams_[i];
        double T = expiries_[i];

        double beta_R = params.b * (1.0 + params.rho);
        double beta_L = params.b * (1.0 - params.rho);
        if (beta_R > 2.0 || beta_L > 2.0) {
            violations.push_back("Lee's Moment Formula violated (beta_R=" + std::to_string(beta_R) 
                + ", beta_L=" + std::to_string(beta_L) + ", both must be <= 2) at expiry " + std::to_string(T));
        }

        bool hasButterfly = false;
        for (double k = -2.0; k <= 2.0; k += 0.05) {
            double w = params.totalVariance(k);
            if (w <= 0.0) {
                hasButterfly = true;
                break;
            }
            double u = k - params.m;
            double s = std::sqrt(u * u + params.sigma * params.sigma);
            double dw_dk = params.b * (params.rho + u / s);
            double d2w_dk2 = params.b * params.sigma * params.sigma / (s * s * s);
            
            double g_k = std::pow(1.0 - k * dw_dk / (2.0 * w), 2.0) 
                         - (dw_dk * dw_dk / 4.0) * (1.0 / w + 0.25) 
                         + d2w_dk2 / 2.0;
                         
            if (g_k < 0) {
                hasButterfly = true;
                break;
            }
        }
        if (hasButterfly) {
            violations.push_back("Durrleman's density condition violated (g(k) < 0) at expiry " + std::to_string(T));
        }

        if (i > 0) {
            bool hasCalendar = false;
            for (double k = -2.0; k <= 2.0; k += 0.05) {
                double w_current = params.totalVariance(k);
                double w_prev = sviParams_[i-1].totalVariance(k);
                if (w_current < w_prev) {
                    hasCalendar = true;
                    break;
                }
            }
            if (hasCalendar) {
                violations.push_back("Calendar arbitrage (w_T2 < w_T1) at expiry " + std::to_string(T));
            }
        }

        if (std::abs(params.rho) > 1.0) {
            violations.push_back("Invalid correlation parameter at expiry " + std::to_string(T));
        }
        if (params.sigma <= 0.0) {
            violations.push_back("Non-positive curvature parameter at expiry " + std::to_string(T));
        }
        if (params.b < 0.0) {
            violations.push_back("Negative slope parameter b at expiry " + std::to_string(T));
        }
    }
    
    return violations;
}

void SVISurface::print() const {
    std::cout << "\n=== SVI Volatility Surface Parameters ===\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::setw(8) << "Expiry" << std::setw(8) << "a" << std::setw(8) << "b" 
              << std::setw(8) << "rho" << std::setw(8) << "m" << std::setw(8) << "sigma" << "\n";
    
    for (size_t i = 0; i < sviParams_.size(); ++i) {
        const auto& params = sviParams_[i];
        std::cout << std::setw(8) << expiries_[i]
                  << std::setw(8) << params.a
                  << std::setw(8) << params.b  
                  << std::setw(8) << params.rho
                  << std::setw(8) << params.m
                  << std::setw(8) << params.sigma << "\n";
    }
    
    auto violations = getArbitrageViolations();
    if (violations.empty()) {
        std::cout << "\n Surface is arbitrage-free\n";
    } else {
        std::cout << "\n Arbitrage violations found:\n";
        for (const auto& viol : violations) {
            std::cout << "  - " << viol << "\n";
        }
    }
    std::cout << "\n";
}

SVIParams SVISurface::fitSVI(const std::vector<Quote>& quotes, double expiry) const {
    
    std::vector<std::pair<double, double>> data;
    double forward = forwardPrice(expiry);
    
    for (const auto& q : quotes) {
        double logMoneyness = std::log(q.strike / forward);
        double totalVar = q.iv * q.iv * expiry;
        data.emplace_back(logMoneyness, totalVar);
    }

    SVIParams initial;
    initial.a = 0.04;  
    initial.b = 0.4;   
    initial.rho = -0.7; 
    initial.m = 0.0;   
    initial.sigma = 0.2; 

    SVIParams result = calibrateSVI(data);

    return enforceArbitrageConstraints(result, expiry);
}

SVIParams SVISurface::calibrateSVI(const std::vector<std::pair<double, double>>& logMoneynessVariance) const {

    double sum_w = 0, sum_wk = 0, sum_wk2 = 0, sum_wv = 0, sum_wkv = 0;
    
    for (const auto& [k, v] : logMoneynessVariance) {
        double weight = weightFunction(k);
        sum_w += weight;
        sum_wk += weight * k;
        sum_wk2 += weight * k * k;
        sum_wv += weight * v;
        sum_wkv += weight * k * v;
    }
    
    double mean_k = sum_wk / sum_w;
    double mean_v = sum_wv / sum_w;
    
    double num = sum_wkv - sum_w * mean_k * mean_v;
    double den = sum_wk2 - sum_w * mean_k * mean_k;
    
    double slope = (den > 1e-10) ? num / den : 0.0;
    double intercept = mean_v - slope * mean_k;

    SVIParams initialGuess;
    initialGuess.a = std::max(0.001, intercept);
    initialGuess.b = std::max(0.1, std::abs(slope));
    initialGuess.rho = (slope < 0) ? -0.7 : 0.3;
    initialGuess.m = mean_k;
    initialGuess.sigma = 0.2;

    std::vector<double> weights;
    weights.reserve(logMoneynessVariance.size());
    for (const auto& [k, v] : logMoneynessVariance) {
        weights.push_back(weightFunction(k));
    }

    SVICalibrator calibrator;
    SVICalibrator::Options opts;
    opts.maxIterations = 100;
    opts.tolerance = 1e-8;
    opts.verbose = false;  
    calibrator.setOptions(opts);
    
    SVICalibrator::Result result = calibrator.calibrate(logMoneynessVariance, weights, initialGuess);

    if (result.converged) {
        return result.params;
    } else {
        
        std::cerr << "Warning: SVI calibration did not fully converge. "
                  << result.message << " (residual=" << result.finalResidual << ")" << std::endl;
        return result.params;  
    }
}

SVIParams SVISurface::enforceArbitrageConstraints(const SVIParams& params, double ) const {
    SVIParams constrained = params;

    constrained.b = std::max(0.0, constrained.b);
    constrained.rho = std::clamp(constrained.rho, -0.999, 0.999);
    constrained.sigma = std::max(1e-4, constrained.sigma);

    double min_variance = constrained.a + constrained.b * constrained.sigma * std::sqrt(1.0 - constrained.rho * constrained.rho);
    if (min_variance < 1e-4) {
        constrained.a = 1e-4 - constrained.b * constrained.sigma * std::sqrt(1.0 - constrained.rho * constrained.rho);
    }

    double max_wing = std::max(1.0 + constrained.rho, 1.0 - constrained.rho);
    if (constrained.b * max_wing > 2.0) {
        constrained.b = 2.0 / max_wing;
    }
    
    return constrained;
}

double SVISurface::weightFunction(double logMoneyness) const {
    
    double atm_weight = 1.0;
    double wing_weight = 0.1;
    double decay_rate = 2.0;
    
    return wing_weight + (atm_weight - wing_weight) * std::exp(-decay_rate * logMoneyness * logMoneyness);
}

double SVISurface::forwardPrice(double expiry) const {
    return marketData_.spot * std::exp((marketData_.riskFreeRate - marketData_.dividendYield) * expiry);
}

Eigen::VectorXd SVICalibrator::paramsToVector(const SVIParams& params) const {
    Eigen::VectorXd theta(5);
    theta << params.a, params.b, params.rho, params.m, params.sigma;
    return theta;
}

SVIParams SVICalibrator::vectorToParams(const Eigen::VectorXd& theta) const {
    SVIParams params;
    params.a = theta(0);
    params.b = theta(1);
    params.rho = theta(2);
    params.m = theta(3);
    params.sigma = theta(4);
    return params;
}

SVIParams SVICalibrator::projectToFeasible(const SVIParams& params) const {
    SVIParams proj = params;

    proj.b = std::max(1e-6, proj.b);           
    proj.rho = std::clamp(proj.rho, -0.999, 0.999);  
    proj.sigma = std::max(1e-6, proj.sigma);   

    double minVariance = proj.a + proj.b * proj.sigma * std::sqrt(1.0 - proj.rho * proj.rho);
    if (minVariance < 1e-6) {
        proj.a = std::max(proj.a, 1e-6 - proj.b * proj.sigma * std::sqrt(1.0 - proj.rho * proj.rho));
    }

    double max_wing = std::max(1.0 + proj.rho, 1.0 - proj.rho);
    if (proj.b * max_wing > 2.0) {
        proj.b = 2.0 / max_wing;
    }
    
    return proj;
}

Eigen::VectorXd SVICalibrator::computeResiduals(
    const std::vector<std::pair<double, double>>& data,
    const std::vector<double>& weights,
    const SVIParams& params) const 
{
    int n = static_cast<int>(data.size());
    Eigen::VectorXd r(n);
    
    for (int i = 0; i < n; ++i) {
        double k = data[i].first;        
        double w_obs = data[i].second;   
        double w_model = params.totalVariance(k);
        double wt = (i < static_cast<int>(weights.size())) ? weights[i] : 1.0;
        r(i) = wt * (w_model - w_obs);
    }
    
    return r;
}

Eigen::MatrixXd SVICalibrator::computeJacobian(
    const std::vector<std::pair<double, double>>& data,
    const std::vector<double>& weights,
    const SVIParams& params) const 
{
    int n = static_cast<int>(data.size());
    Eigen::MatrixXd J(n, 5);
    
    for (int i = 0; i < n; ++i) {
        double k = data[i].first;
        double wt = (i < static_cast<int>(weights.size())) ? weights[i] : 1.0;
        
        double u = k - params.m;
        double sqrt_term = std::sqrt(u * u + params.sigma * params.sigma);

        J(i, 0) = wt * 1.0;                                          
        J(i, 1) = wt * (params.rho * u + sqrt_term);                 
        J(i, 2) = wt * params.b * u;                                 
        J(i, 3) = wt * params.b * (-params.rho - u / sqrt_term);     
        J(i, 4) = wt * params.b * params.sigma / sqrt_term;          
    }
    
    return J;
}

Eigen::VectorXd SVICalibrator::solveLMStep(
    const Eigen::MatrixXd& J,
    const Eigen::VectorXd& r,
    double lambda) const 
{
    
    int nParams = static_cast<int>(J.cols());
    
    Eigen::MatrixXd JtJ = J.transpose() * J;
    Eigen::VectorXd Jtr = J.transpose() * r;

    for (int i = 0; i < nParams; ++i) {
        JtJ(i, i) += lambda * (1.0 + JtJ(i, i));  
    }

    Eigen::LLT<Eigen::MatrixXd> llt(JtJ);
    if (llt.info() != Eigen::Success) {
        
        return JtJ.ldlt().solve(-Jtr);
    }
    
    return llt.solve(-Jtr);
}

bool SVICalibrator::checkConvergence(
    const Eigen::VectorXd& ,
    const Eigen::VectorXd& delta,
    double residualNorm) const 
{
    
    if (residualNorm < options_.tolerance) {
        return true;
    }

    double deltaNorm = delta.norm();
    if (deltaNorm < options_.paramTolerance) {
        return true;
    }
    
    return false;
}

SVICalibrator::Result SVICalibrator::calibrate(
    const std::vector<std::pair<double, double>>& data,
    const std::vector<double>& weights,
    const SVIParams& initialGuess) 
{
    Result result;
    result.params = initialGuess;
    result.converged = false;
    result.iterations = 0;

    Eigen::VectorXd theta = paramsToVector(initialGuess);
    double lambda = options_.initialDamping;

    Eigen::VectorXd r = computeResiduals(data, weights, result.params);
    double residualNorm = r.norm();
    double prevResidualNorm = residualNorm;
    
    if (options_.verbose) {
        std::cout << "LM Iteration 0: residual = " << residualNorm << std::endl;
    }

    for (int iter = 0; iter < options_.maxIterations; ++iter) {
        result.iterations = iter + 1;

        Eigen::MatrixXd J = computeJacobian(data, weights, result.params);

        Eigen::VectorXd delta = solveLMStep(J, r, lambda);

        Eigen::VectorXd theta_new = theta + delta;
        SVIParams params_new = vectorToParams(theta_new);
        params_new = projectToFeasible(params_new);

        Eigen::VectorXd r_new = computeResiduals(data, weights, params_new);
        double residualNorm_new = r_new.norm();

        double actualReduction = residualNorm * residualNorm - residualNorm_new * residualNorm_new;
        Eigen::VectorXd predictedResidual = r - J * delta;
        double predictedReduction = residualNorm * residualNorm - predictedResidual.squaredNorm();
        double rho = (predictedReduction > 1e-10) ? actualReduction / predictedReduction : 0.0;
        
        if (rho > 0) {
            
            theta = theta_new;
            result.params = params_new;
            r = r_new;
            residualNorm = residualNorm_new;
            lambda = std::max(lambda * options_.dampingDownFactor, options_.minDamping);
            
            if (options_.verbose) {
                std::cout << "LM Iteration " << iter + 1 << ": residual = " << residualNorm 
                          << ", lambda = " << lambda << " (accepted)" << std::endl;
            }

            if (checkConvergence(r, delta, residualNorm)) {
                result.converged = true;
                result.finalResidual = residualNorm;
                result.finalResidualNorm = residualNorm;
                result.message = "Converged: residual or parameter change below tolerance";
                break;
            }
            
        } else {
            
            lambda = std::min(lambda * options_.dampingUpFactor, options_.maxDamping);
            
            if (options_.verbose) {
                std::cout << "LM Iteration " << iter + 1 << ": residual = " << residualNorm 
                          << ", lambda = " << lambda << " (rejected)" << std::endl;
            }
        }

        if (std::abs(residualNorm - prevResidualNorm) < 1e-12) {
            result.message = "Stagnated: no improvement in residual";
            break;
        }
        prevResidualNorm = residualNorm;

        if (lambda > options_.maxDamping) {
            result.message = "Failed: damping parameter exceeded maximum";
            break;
        }
    }
    
    if (!result.converged && result.iterations >= options_.maxIterations) {
        result.message = "Failed: maximum iterations reached";
    }
    
    result.finalResidual = residualNorm;
    result.finalResidualNorm = residualNorm;
    
    return result;
}
