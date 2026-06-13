

#pragma once
#include "vol_surface.hpp"
#include <vector>
#include <Eigen/Dense>

struct SVIParams {
    double a;      
    double b;      
    double rho;    
    double m;      
    double sigma;  

    bool isValid() const;

    double totalVariance(double logMoneyness) const;

    double impliedVol(double logMoneyness, double expiry) const;
};

class SVICalibrator {
public:
    
    struct Result {
        SVIParams params;          
        bool converged;            
        int iterations;            
        double finalResidual;      
        double finalResidualNorm;  
        std::string message;       
    };

    struct Options {
        int maxIterations = 100;       
        double tolerance = 1e-8;       
        double paramTolerance = 1e-8;  
        double initialDamping = 1e-3;  
        double dampingDownFactor = 0.1; 
        double dampingUpFactor = 10.0; 
        double minDamping = 1e-10;     
        double maxDamping = 1e10;      
        bool verbose = false;          
    };

    explicit SVICalibrator(const Options& opts = Options()) : options_(opts) {}

    Result calibrate(
        const std::vector<std::pair<double, double>>& data,
        const std::vector<double>& weights,
        const SVIParams& initialGuess);

    void setOptions(const Options& opts) { options_ = opts; }

    const Options& getOptions() const { return options_; }

private:
    Options options_;

    Eigen::VectorXd paramsToVector(const SVIParams& params) const;
    SVIParams vectorToParams(const Eigen::VectorXd& theta) const;

    Eigen::VectorXd computeResiduals(
        const std::vector<std::pair<double, double>>& data,
        const std::vector<double>& weights,
        const SVIParams& params) const;

    Eigen::MatrixXd computeJacobian(
        const std::vector<std::pair<double, double>>& data,
        const std::vector<double>& weights,
        const SVIParams& params) const;

    Eigen::VectorXd solveLMStep(
        const Eigen::MatrixXd& J,
        const Eigen::VectorXd& r,
        double lambda) const;

    SVIParams projectToFeasible(const SVIParams& params) const;

    bool checkConvergence(
        const Eigen::VectorXd& r,
        const Eigen::VectorXd& delta,
        double residualNorm) const;
};

class SVISurface {
public:
    
    explicit SVISurface(const std::vector<Quote>& quotes, const MarketData& marketData);

    double impliedVol(double strike, double expiry) const;

    const MarketData& marketData() const { return marketData_; }

    const std::vector<double>& expiries() const { return expiries_; }

    const std::vector<SVIParams>& sviParams() const { return sviParams_; }

    bool isArbitrageFree() const;

    std::vector<std::string> getArbitrageViolations() const;

    void print() const;

private:
    MarketData marketData_;
    std::vector<double> expiries_;
    std::vector<SVIParams> sviParams_;
    
    SVIParams fitSVI(const std::vector<Quote>& quotes, double expiry) const;
    SVIParams calibrateSVI(const std::vector<std::pair<double, double>>& logMoneynessVariance) const;
    SVIParams enforceArbitrageConstraints(const SVIParams& params, double expiry) const;
    
    double weightFunction(double logMoneyness) const;
    double forwardPrice(double expiry) const;
};
