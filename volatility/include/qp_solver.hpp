

#pragma once
#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <vector>
#include <tuple>
#include "osqp.h"

struct QPResult {
    bool success;              
    Eigen::VectorXd ivFlat;    
    double objectiveValue;     
    double regularizationPenalty; 
    int iterations;            
    std::string status;        
    double solveTime;          
};

class QPSolver {
public:
    
    enum class ObjectiveType {
        L2_DISTANCE,          
        WEIGHTED_L2,          
        HUBER,                
        FAIRNESS_PENALTY,     
        MARKET_PRESERVATION   
    };

    struct Config {
        ObjectiveType objective = ObjectiveType::WEIGHTED_L2;
        double regularizationWeight = 1e-6;     
        double smoothnessWeight = 1e-4;         
        double huberThreshold = 0.01;           
        bool enableVolumeWeighting = true;      
        double minVol = 0.001;                  
        double maxVol = 5.0;                    
        double tolerance = 1e-9;                
        int maxIterations = 10000;              
        bool enableAdaptiveRegularization = true;
        bool verbose = false;                   

        bool enableNonlinearCalendarCheck = true;   
        bool enableCalendarRefinement = false;      
        double calendarViolationTol = 1e-6;         
    };

    explicit QPSolver(const Config& config = Config());
    ~QPSolver();

    void setup(const VolSurface& templateSurface);

    QPResult solve(const VolSurface& surface);

    VolSurface buildCorrectedSurface(const VolSurface& surface, const QPResult& result) const;

    void setConfig(const Config& config) { config_ = config; }

    const Config& getConfig() const { return config_; }

    void buildConstraints(
        const VolSurface& surface,
        Eigen::SparseMatrix<double>& A,
        Eigen::VectorXd& lb,
        Eigen::VectorXd& ub) const;

    void addSmoothnessConstraints(
        std::vector<Eigen::Triplet<double>>& trips,
        std::vector<double>& lb,
        std::vector<double>& ub,
        int& row) const;

    double calculateSmoothnessPenalty(const Eigen::VectorXd& ivFlat) const;



    QPResult refineCalendarConstraint(
        const VolSurface& surface,
        const QPResult& initialResult,
        int maxRefinementIterations = 5) const;

private:
    Config config_;
    
    int cached_nE_ = 0;
    int cached_nK_ = 0;
    
    OSQPSolver* solver_ = nullptr;
    OSQPCscMatrix* P_csc_ = nullptr;
    OSQPCscMatrix* A_csc_ = nullptr;
    OSQPSettings* settings_ = nullptr;

    int nStrikes() const;
    int nExpiries() const;
    int idx(int ei, int ki) const;  

    void addButterflyRow(
        std::vector<Eigen::Triplet<double>>& trips,
        std::vector<double>& lb,
        std::vector<double>& ub,
        int& row, int ei, int ki) const;
        
    void addCalendarRow(
        const VolSurface& surface,
        std::vector<Eigen::Triplet<double>>& trips,
        std::vector<double>& lb,
        std::vector<double>& ub,
        int& row, int ei, int ki) const;

    void buildObjective(
        Eigen::SparseMatrix<double>& P,
        Eigen::VectorXd& q,
        const Eigen::VectorXd& ivMarket) const;

    Eigen::VectorXd calculateWeights() const;

    double calculateAdaptiveRegularization(const Eigen::VectorXd& ivMarket) const;

    OSQPSettings* createOptimizedSettings() const;

    double computeCalendarViolation(
        const VolSurface& surface,
        const Eigen::VectorXd& ivFlat,
        int expiry0_idx,
        int expiry1_idx,
        int strike_idx) const;

    bool verifyCalendarConstraint(
        const VolSurface& surface,
        const Eigen::VectorXd& ivFlat,
        std::vector<std::tuple<int, int, double>>& violations) const;

    double calculateMarketPreservationPenalty(const VolSurface& surface, const Eigen::VectorXd& ivFlat) const;

    void cleanupWorkspace();
};
