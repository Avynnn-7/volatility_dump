#include "vol_surface.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool VolSurface::getCached(const CacheKey& key, double& value) const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex_);
    
    auto it = cacheMap_.find(key);
    if (it == cacheMap_.end()) {
        ++cacheStats_.misses;
        return false;
    }

    lock.unlock();
    std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);

    it = cacheMap_.find(key);
    if (it == cacheMap_.end()) {
        ++cacheStats_.misses;
        return false;
    }
    
    value = it->second->second;
    cacheList_.splice(cacheList_.begin(), cacheList_, it->second);
    ++cacheStats_.hits;
    return true;
}

void VolSurface::putCache(const CacheKey& key, double value) const {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);

    auto it = cacheMap_.find(key);
    if (it != cacheMap_.end()) {
        
        it->second->second = value;
        cacheList_.splice(cacheList_.begin(), cacheList_, it->second);
        return;
    }

    while (cacheList_.size() >= maxCacheSize_) {
        auto last = cacheList_.back();
        cacheMap_.erase(last.first);
        cacheList_.pop_back();
        ++cacheStats_.evictions;
    }

    cacheList_.emplace_front(key, value);
    cacheMap_[key] = cacheList_.begin();
}

VolSurface::CacheStats VolSurface::getCacheStats() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex_);
    return cacheStats_;
}

void VolSurface::clearCache() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);
    cacheList_.clear();
    cacheMap_.clear();
    cacheStats_ = CacheStats{};
}

void VolSurface::setCacheSize(size_t maxEntries) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);
    maxCacheSize_ = maxEntries;

    while (cacheList_.size() > maxCacheSize_) {
        auto last = cacheList_.back();
        cacheMap_.erase(last.first);
        cacheList_.pop_back();
        ++cacheStats_.evictions;
    }
}

VolSurface::VolSurface(const std::vector<Quote>& quotes, const MarketData& marketData)
    : marketData_(marketData)
{
    if (quotes.empty())
        throw std::invalid_argument("VolSurface: no quotes supplied");

    if (marketData_.spot <= 0.0)
        throw std::invalid_argument("VolSurface: spot price must be positive");
    if (marketData_.riskFreeRate < -0.5 || marketData_.riskFreeRate > 1.0)
        throw std::invalid_argument("VolSurface: risk-free rate out of reasonable range [-50%, 100%]");
    if (marketData_.dividendYield < 0.0 || marketData_.dividendYield > 0.2)
        throw std::invalid_argument("VolSurface: dividend yield out of reasonable range [0%, 20%]");

    for (const auto& q : quotes) {
        if (q.strike <= 0.0)
            throw std::invalid_argument("VolSurface: strike must be positive");
        if (q.expiry <= 0.0)
            throw std::invalid_argument("VolSurface: expiry must be positive");
        if (q.iv <= 0.0 || q.iv > 5.0)
            throw std::invalid_argument("VolSurface: IV out of reasonable range (0%, 500%]");
        
        strikes_.push_back(q.strike);
        expiries_.push_back(q.expiry);
    }
    std::sort(strikes_.begin(),  strikes_.end());
    std::sort(expiries_.begin(), expiries_.end());
    strikes_.erase(std::unique(strikes_.begin(),  strikes_.end()),  strikes_.end());
    expiries_.erase(std::unique(expiries_.begin(), expiries_.end()), expiries_.end());

    const int nE = static_cast<int>(expiries_.size());
    const int nK = static_cast<int>(strikes_.size());

    ivGrid_ = Eigen::MatrixXd::Constant(nE, nK, std::numeric_limits<double>::quiet_NaN());

    for (const auto& q : quotes) {
        int ei = static_cast<int>(
            std::lower_bound(expiries_.begin(), expiries_.end(), q.expiry) - expiries_.begin());
        int ki = static_cast<int>(
            std::lower_bound(strikes_.begin(),  strikes_.end(),  q.strike) - strikes_.begin());
        ivGrid_(ei, ki) = q.iv;
    }

    for (int i = 0; i < nE; ++i)
        for (int j = 0; j < nK; ++j)
            if (std::isnan(ivGrid_(i, j))) {
                double weightedSum = 0.0, totalWeight = 0.0;
                for (const auto& q : quotes) {
                    double dist = std::hypot(expiries_[i] - q.expiry, (strikes_[j] - q.strike) / marketData_.spot);
                    double weight = std::exp(-dist * dist) * (q.volume > 0 ? q.volume : 1.0);
                    weightedSum += weight * q.iv;
                    totalWeight += weight;
                }
                ivGrid_(i, j) = totalWeight > 0 ? weightedSum / totalWeight : 0.20;
            }
}

double VolSurface::impliedVol(double strike, double expiry) const {
    
    CacheKey key{strike, expiry, CacheKey::Type::IV};
    double cached;
    if (getCached(key, cached)) {
        return cached;
    }

    double result = impliedVolUncached(strike, expiry);

    putCache(key, result);
    return result;
}

double VolSurface::impliedVolUncached(double strike, double expiry) const {
    double K = std::clamp(strike,  strikes_.front(), strikes_.back());
    double T = std::clamp(expiry,  expiries_.front(), expiries_.back());

    int nK = static_cast<int>(strikes_.size());
    int nE = static_cast<int>(expiries_.size());

    int ki = static_cast<int>(
        std::lower_bound(strikes_.begin(),  strikes_.end(),  K) - strikes_.begin());
    int ei = static_cast<int>(
        std::lower_bound(expiries_.begin(), expiries_.end(), T) - expiries_.begin());

    ki = std::min(ki, nK - 1);
    ei = std::min(ei, nE - 1);
    int ki0 = std::max(ki - 1, 0), ki1 = ki;
    int ei0 = std::max(ei - 1, 0), ei1 = ei;

    double K0 = strikes_[ki0],  K1 = strikes_[ki1];
    double T0 = expiries_[ei0], T1 = expiries_[ei1];
    double wK = (K1 > K0) ? (K - K0) / (K1 - K0) : 0.0;
    double wT = (T1 > T0) ? (T - T0) / (T1 - T0) : 0.0;

    double var00 = ivGrid_(ei0, ki0) * ivGrid_(ei0, ki0) * T0;
    double var01 = ivGrid_(ei0, ki1) * ivGrid_(ei0, ki1) * T0;
    double var10 = ivGrid_(ei1, ki0) * ivGrid_(ei1, ki0) * T1;
    double var11 = ivGrid_(ei1, ki1) * ivGrid_(ei1, ki1) * T1;

    double var_interp = (1-wT)*((1-wK)*var00 + wK*var01)
                      +    wT *((1-wK)*var10 + wK*var11);

    return std::sqrt(std::max(0.0, var_interp) / std::max(1e-8, T));
}

double VolSurface::normalCDF(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double VolSurface::normalPDF(double x) {
    return (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * x * x);
}

double VolSurface::bsCall(double S, double K, double T, double sigma, double r, double q) {

    if (T <= 0.0 || sigma <= 0.0) return std::max(S - K, 0.0);

    if (S <= 0.0 || K <= 0.0 || !std::isfinite(S) || !std::isfinite(K)) {
        return 0.0;
    }

    constexpr double MAX_EXPIRY = 100.0;      
    constexpr double MAX_VOL = 10.0;          
    constexpr double MAX_RATE = 1.0;          
    constexpr double MAX_D = 10.0;            
    
    T = std::min(T, MAX_EXPIRY);
    sigma = std::clamp(sigma, 1e-6, MAX_VOL);
    r = std::clamp(r, -MAX_RATE, MAX_RATE);
    q = std::clamp(q, -MAX_RATE, MAX_RATE);
    
    double sqrtT = std::sqrt(T);
    double log_moneyness = std::log(S / K);

    if (!std::isfinite(log_moneyness)) {
        return (S > K) ? S - K : 0.0;
    }
    
    double d1 = (log_moneyness + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    d1 = std::clamp(d1, -MAX_D, MAX_D);
    d2 = std::clamp(d2, -MAX_D, MAX_D);

    double disc_S = std::exp(std::clamp(-q * T, -700.0, 700.0));
    double disc_K = std::exp(std::clamp(-r * T, -700.0, 700.0));
    
    double result = S * disc_S * normalCDF(d1) - K * disc_K * normalCDF(d2);

    return std::isfinite(result) ? std::max(result, 0.0) : std::max(S - K, 0.0);
}

double VolSurface::bsPut(double S, double K, double T, double sigma, double r, double q) {

    if (T <= 0.0 || sigma <= 0.0) return std::max(K - S, 0.0);

    if (S <= 0.0 || K <= 0.0 || !std::isfinite(S) || !std::isfinite(K)) {
        return 0.0;
    }

    constexpr double MAX_EXPIRY = 100.0;      
    constexpr double MAX_VOL = 10.0;          
    constexpr double MAX_RATE = 1.0;          
    constexpr double MAX_D = 10.0;            
    
    T = std::min(T, MAX_EXPIRY);
    sigma = std::clamp(sigma, 1e-6, MAX_VOL);
    r = std::clamp(r, -MAX_RATE, MAX_RATE);
    q = std::clamp(q, -MAX_RATE, MAX_RATE);
    
    double sqrtT = std::sqrt(T);
    double log_moneyness = std::log(S / K);

    if (!std::isfinite(log_moneyness)) {
        return (K > S) ? K - S : 0.0;
    }
    
    double d1 = (log_moneyness + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    d1 = std::clamp(d1, -MAX_D, MAX_D);
    d2 = std::clamp(d2, -MAX_D, MAX_D);

    double disc_S = std::exp(std::clamp(-q * T, -700.0, 700.0));
    double disc_K = std::exp(std::clamp(-r * T, -700.0, 700.0));
    
    double result = K * disc_K * normalCDF(-d2) - S * disc_S * normalCDF(-d1);

    return std::isfinite(result) ? std::max(result, 0.0) : std::max(K - S, 0.0);
}

double VolSurface::forward(double expiry) const {
    return marketData_.spot * std::exp((marketData_.riskFreeRate - marketData_.dividendYield) * expiry);
}

double VolSurface::discountFactor(double expiry) const {
    return std::exp(-marketData_.riskFreeRate * expiry);
}

double VolSurface::callPrice(double strike, double expiry) const {
    
    CacheKey key{strike, expiry, CacheKey::Type::CallPrice};
    double cached;
    if (getCached(key, cached)) {
        return cached;
    }

    double result = callPriceUncached(strike, expiry);

    putCache(key, result);
    return result;
}

double VolSurface::callPriceUncached(double strike, double expiry) const {
    return bsCall(marketData_.spot, strike, expiry, impliedVol(strike, expiry), 
                  marketData_.riskFreeRate, marketData_.dividendYield);
}

double VolSurface::putPrice(double strike, double expiry) const {
    
    CacheKey key{strike, expiry, CacheKey::Type::PutPrice};
    double cached;
    if (getCached(key, cached)) {
        return cached;
    }

    double result = putPriceUncached(strike, expiry);

    putCache(key, result);
    return result;
}

double VolSurface::putPriceUncached(double strike, double expiry) const {
    return bsPut(marketData_.spot, strike, expiry, impliedVol(strike, expiry),
                 marketData_.riskFreeRate, marketData_.dividendYield);
}

void VolSurface::print() const {
    std::cout << "\n=== Implied Volatility Surface (%) ===\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << std::setw(8) << "T\\K";
    for (double K : strikes_) std::cout << std::setw(8) << K;
    std::cout << "\n";
    for (int i = 0; i < (int)expiries_.size(); ++i) {
        std::cout << std::setw(8) << expiries_[i];
        for (int j = 0; j < (int)strikes_.size(); ++j)
            std::cout << std::setw(8) << ivGrid_(i, j) * 100.0;
        std::cout << "\n";
    }
    std::cout << "\n";
}
