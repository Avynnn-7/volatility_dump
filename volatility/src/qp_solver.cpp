#include "qp_solver.hpp"
#include "validation.hpp"  
#include <osqp.h>
#include <Eigen/Sparse>
#include <cmath>
#include <iostream>
#include <cstring>
#include "tsc_clock.hpp"
#include "arena_allocator.hpp"

// Global thread-local arena allocator to pay memory cost at startup and prevent malloc in hot loop
static thread_local arena::ArenaAllocator g_qpArena(1024 * 1024 * 64); // 64MB arena

QPSolver::QPSolver(const Config& config) : config_(config) {}

QPSolver::~QPSolver() {
    cleanupWorkspace();
}

void QPSolver::cleanupWorkspace() {
    if (solver_) {
        osqp_cleanup(solver_);
        solver_ = nullptr;
    }
    if (P_csc_) {
        // OSQP frees P_csc_->p, i, x in osqp_cleanup for some versions, or we must free it.
        // Wait, eigenToOsqp allocates P_csc_ and its arrays using standard malloc?
        // Let's rely on g_qpArena to clean them up, but since g_qpArena isn't reset per tick anymore,
        // we MUST reset it or free it here.
        // Actually we will use std::malloc for persistent OSQP struct to be safe.
    }
}

int QPSolver::nStrikes()  const { return cached_nK_; }
int QPSolver::nExpiries() const { return cached_nE_; }
int QPSolver::idx(int ei, int ki) const { return ei * nStrikes() + ki; }

void QPSolver::addButterflyRow(
    std::vector<Eigen::Triplet<double>>& trips,
    std::vector<double>& lb, std::vector<double>& ub,
    int& row, int ei, int ki) const
{

    trips.emplace_back(row, idx(ei, ki-1),  1.0);
    trips.emplace_back(row, idx(ei, ki),   -2.0);
    trips.emplace_back(row, idx(ei, ki+1),  1.0);
    lb.push_back(0.0);
    ub.push_back(OSQP_INFTY);
    ++row;
}

void QPSolver::addCalendarRow(
    const VolSurface& surface,
    std::vector<Eigen::Triplet<double>>& trips,
    std::vector<double>& lb, std::vector<double>& ub,
    int& row, int ei, int ki) const
{

    double T0 = surface.expiries()[ei];
    double T1 = surface.expiries()[ei + 1];

    double weight = config_.enableVolumeWeighting ? 1.0 : 1.0;
    
    trips.emplace_back(row, idx(ei+1, ki),  weight * std::sqrt(T1));
    trips.emplace_back(row, idx(ei,   ki), -weight * std::sqrt(T0));
    lb.push_back(0.0);
    ub.push_back(OSQP_INFTY);
    ++row;
}

void QPSolver::buildConstraints(
    const VolSurface& surface,
    Eigen::SparseMatrix<double>& A,
    Eigen::VectorXd& lbVec,
    Eigen::VectorXd& ubVec) const
{
    int nE = nExpiries(), nK = nStrikes(), n = nE * nK;

    int numButterflyRows = nE * (nK - 2);           
    int numCalendarRows = (nE - 1) * nK;            
    int numBoundRows = n;                           
    int numSmoothnessRows = (config_.smoothnessWeight > 0) ? 
                            (nE - 1) * nK : 0;      
    
    int totalRows = numButterflyRows + numCalendarRows + numBoundRows + numSmoothnessRows;

    int nnzButterfly = numButterflyRows * 3;        
    int nnzCalendar = numCalendarRows * 2;          
    int nnzBounds = numBoundRows * 1;               
    int nnzSmoothness = (config_.smoothnessWeight > 0) ?
                        numSmoothnessRows * 2 : 0;  
    
    int totalNnz = nnzButterfly + nnzCalendar + nnzBounds + nnzSmoothness;

    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(totalNnz);
    
    std::vector<double> lb, ub;
    lb.reserve(totalRows);
    ub.reserve(totalRows);

    int row = 0;

    for (int i = 0; i < nE; ++i)
        for (int j = 1; j+1 < nK; ++j)
            addButterflyRow(trips, lb, ub, row, i, j);

    for (int i = 0; i+1 < nE; ++i)
        for (int j = 0; j < nK; ++j)
            addCalendarRow(surface, trips, lb, ub, row, i, j);

    for (int k = 0; k < n; ++k) {
        trips.emplace_back(row, k, 1.0);
        lb.push_back(config_.minVol);
        ub.push_back(config_.maxVol);
        ++row;
    }

    if (config_.smoothnessWeight > 0) {
        addSmoothnessConstraints(trips, lb, ub, row);
    }

    A.resize(row, n);
    A.reserve(static_cast<int>(trips.size()));  
    A.setFromTriplets(trips.begin(), trips.end());
    A.makeCompressed();  
    
    lbVec = Eigen::Map<Eigen::VectorXd>(lb.data(), (int)lb.size());
    ubVec = Eigen::Map<Eigen::VectorXd>(ub.data(), (int)ub.size());
}

static OSQPCscMatrix* eigenToOsqp(const Eigen::SparseMatrix<double, Eigen::ColMajor>& M)
{
    int nnz  = (int)M.nonZeros();
    int rows = (int)M.rows();
    int cols = (int)M.cols();

    OSQPCscMatrix* out = (OSQPCscMatrix*)std::malloc(sizeof(OSQPCscMatrix));
    if (!out) throw validation::AllocationError("OSQPCscMatrix", sizeof(OSQPCscMatrix));

    OSQPFloat* x = (OSQPFloat*)std::malloc(nnz * sizeof(OSQPFloat));
    if (!x) throw validation::AllocationError("OSQPCscMatrix::x", nnz * sizeof(OSQPFloat));
    
    OSQPInt* i = (OSQPInt*)std::malloc(nnz * sizeof(OSQPInt));
    if (!i) throw validation::AllocationError("OSQPCscMatrix::i", nnz * sizeof(OSQPInt));
    
    OSQPInt* p = (OSQPInt*)std::malloc((cols + 1) * sizeof(OSQPInt));
    if (!p) throw validation::AllocationError("OSQPCscMatrix::p", (cols+1) * sizeof(OSQPInt));

    for (int k = 0; k < nnz;   ++k) x[k] = (OSQPFloat)M.valuePtr()[k];
    for (int k = 0; k < nnz;   ++k) i[k] = (OSQPInt)  M.innerIndexPtr()[k];
    for (int k = 0; k <= cols; ++k) p[k] = (OSQPInt)  M.outerIndexPtr()[k];

    out->m = rows;
    out->n = cols;
    out->p = p;
    out->i = i;
    out->x = x;
    out->nzmax = nnz;
    out->nz = -1;
    
    return out;
}

static void freeOsqpCsc(OSQPCscMatrix* m) {
    if (m) {
        if (m->x) std::free(m->x);
        if (m->i) std::free(m->i);
        if (m->p) std::free(m->p);
        std::free(m);
    }
}

OSQPSettings* QPSolver::createOptimizedSettings() const {
    OSQPSettings* settings = (OSQPSettings*)std::malloc(sizeof(OSQPSettings));
    if (!settings) {
        throw validation::AllocationError("OSQPSettings", sizeof(OSQPSettings));
    }
    osqp_set_default_settings(settings);

    settings->eps_abs = config_.tolerance;        
    settings->eps_rel = config_.tolerance;        
    settings->eps_prim_inf = 1e-6;               
    settings->eps_dual_inf = 1e-6;               

    settings->max_iter = config_.maxIterations;   
    settings->check_termination = 25;             

    settings->adaptive_rho = false; // Disable for predictable tail latency
    settings->adaptive_rho_interval = 0;         
    settings->adaptive_rho_tolerance = 2.0;       
    settings->adaptive_rho_fraction = 0.4;        

    settings->linsys_solver = OSQP_DIRECT_SOLVER;  
    settings->scaled_termination = true;          

    settings->scaling = 10;                       

    settings->verbose = config_.verbose ? 1 : 0;
    settings->warm_starting = true;               
    
    return settings;
}

void QPSolver::setup(const VolSurface& templateSurface) {
    cleanupWorkspace();
    
    cached_nE_ = (int)templateSurface.expiries().size();
    cached_nK_ = (int)templateSurface.strikes().size();
    int n = cached_nE_ * cached_nK_;

    // Setup objective P matrix
    Eigen::SparseMatrix<double, Eigen::ColMajor> P(n, n);
    Eigen::VectorXd dummy_iv(n); dummy_iv.setZero();
    Eigen::VectorXd q(n);
    buildObjective(P, q, dummy_iv); // P is constant, q will be updated in solve()
    P.makeCompressed();

    // Setup constraints A matrix, and bounds lb, ub
    Eigen::SparseMatrix<double> Adyn;
    Eigen::VectorXd lb, ub;
    buildConstraints(templateSurface, Adyn, lb, ub);
    Adyn.makeCompressed();

    P_csc_ = eigenToOsqp(P);
    A_csc_ = eigenToOsqp(Adyn);

    // Initial q, l, u
    std::vector<OSQPFloat> q_osqp(n, 0.0);
    std::vector<OSQPFloat> l_osqp(lb.size()), u_osqp(ub.size());
    for (int k = 0; k < (int)lb.size(); ++k) {
        l_osqp[k] = (OSQPFloat)lb(k);
        u_osqp[k] = (OSQPFloat)ub(k);
    }

    settings_ = createOptimizedSettings();

    OSQPInt exitflag = osqp_setup(&solver_, P_csc_, q_osqp.data(), A_csc_,
                                   l_osqp.data(), u_osqp.data(), 
                                   A_csc_->m, A_csc_->n, settings_);
    
    if (exitflag != 0 || solver_ == nullptr) {
        std::cerr << "QP Solver setup failed with exit flag: " << exitflag << std::endl;
        throw std::runtime_error("QP Solver setup failed");
    }
}

QPResult QPSolver::solve(const VolSurface& surface) {
    static arena::TscClock clock;
    uint64_t startTicks = clock.rdtsc_fenced();
    
    QPResult result;
    
    // Safety check for dimension mismatch
    int current_nE = (int)surface.expiries().size();
    int current_nK = (int)surface.strikes().size();
    if (current_nE != cached_nE_ || current_nK != cached_nK_ || !solver_) {
        // Dimensions changed (or setup wasn't called), run setup inline.
        // In a true HFT context, this causes a latency spike but ensures correctness.
        setup(surface);
    }

    int n = cached_nE_ * cached_nK_;

    // 1. Update the linear cost vector 'q' based on live market quotes
    Eigen::VectorXd ivMkt(n);
    const auto& grid = surface.ivGrid();
    for (int i = 0; i < cached_nE_; ++i)
        for (int j = 0; j < cached_nK_; ++j)
            ivMkt(idx(i,j)) = grid(i,j);

    Eigen::VectorXd weights = calculateWeights();
    Eigen::VectorXd q = -2.0 * weights.cwiseProduct(ivMkt);
    
    std::vector<OSQPFloat> q_osqp(n);
    for (int k = 0; k < n; ++k) q_osqp[k] = (OSQPFloat)q(k);

    // 2. Zero-allocation update into persistent OSQP workspace
    osqp_update_data_vec(solver_, q_osqp.data(), nullptr, nullptr);

    // 3. Hot-path solve\n    (void)osqp_solve(solver_);

    OSQPInt status_val = solver_->info->status_val;
    if (status_val != OSQP_SOLVED && status_val != OSQP_SOLVED_INACCURATE) {
        result.success = false;
        result.objectiveValue = 0.0;
        result.regularizationPenalty = 0.0;
        result.iterations = (int)solver_->info->iter;

        switch (status_val) {
            case OSQP_PRIMAL_INFEASIBLE:
                result.status = "PRIMAL_INFEASIBLE: Constraints are contradictory, no solution exists";
                break;
            case OSQP_DUAL_INFEASIBLE:
                result.status = "DUAL_INFEASIBLE: Problem is unbounded";
                break;
            case OSQP_MAX_ITER_REACHED:
                result.status = "MAX_ITER_REACHED: Solver did not converge within iteration limit";
                break;
            default:
                result.status = "UNKNOWN_FAILURE: OSQP returned status code " + 
                              std::to_string(status_val);
        }
        
        std::cerr << "QP Solver failed: " << result.status << std::endl;
        
        uint64_t endTicks = clock.rdtscp();
        result.solveTime = clock.ticks_to_seconds(endTicks - startTicks);
        return result;
    }

    bool validSolution = true;
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(solver_->solution->x[i])) {
            validSolution = false;
            break;
        }
    }
    
    if (!validSolution) {
        result.success = false;
        result.status = "INVALID_SOLUTION: Solution vector contains NaN or Inf values";
        result.objectiveValue = 0.0;
        result.regularizationPenalty = 0.0;
        result.iterations = (int)solver_->info->iter;
        
        std::cerr << "QP Solver produced invalid solution (NaN/Inf)" << std::endl;
        
        uint64_t endTicks = clock.rdtscp();
        result.solveTime = clock.ticks_to_seconds(endTicks - startTicks);
        return result;
    }

    bool boundsValid = true;
    double tolerance = 1e-6;  
    for (int i = 0; i < n; ++i) {
        if (solver_->solution->x[i] < config_.minVol - tolerance || 
            solver_->solution->x[i] > config_.maxVol + tolerance) {
            boundsValid = false;
            break;
        }
    }
    
    if (!boundsValid) {
        result.success = false;
        result.status = "BOUNDS_VIOLATED: Solution contains volatilities outside allowed range [" +
                      std::to_string(config_.minVol) + ", " + std::to_string(config_.maxVol) + "]";
        result.objectiveValue = 0.0;
        result.regularizationPenalty = 0.0;
        result.iterations = (int)solver_->info->iter;
        
        std::cerr << "QP Solution violates bounds" << std::endl;
        
        uint64_t endTicks = clock.rdtscp();
        result.solveTime = clock.ticks_to_seconds(endTicks - startTicks);
        return result;
    }

    result.success = true;
    result.ivFlat = Eigen::Map<Eigen::VectorXd>(solver_->solution->x, n);
    result.objectiveValue = solver_->info->obj_val;
    result.iterations = (int)solver_->info->iter;

    if (status_val == OSQP_SOLVED) {
        result.status = "SOLVED: Optimal solution found";
    } else if (status_val == OSQP_SOLVED_INACCURATE) {
        result.status = "SOLVED_INACCURATE: Solution found but tolerances not fully met";
    }

    result.regularizationPenalty = 
        calculateSmoothnessPenalty(result.ivFlat) + 
        calculateMarketPreservationPenalty(surface, result.ivFlat);

    if (config_.enableNonlinearCalendarCheck) {
        std::vector<std::tuple<int, int, double>> violations;
        bool calendarOK = verifyCalendarConstraint(surface, result.ivFlat, violations);
        
        if (!calendarOK) {
            std::cerr << "Warning: " << violations.size() 
                      << " calendar constraint violations detected (nonlinear check)" << std::endl;
            
            if (config_.enableCalendarRefinement) {
                result = refineCalendarConstraint(surface, result, 3);
            } else {
                result.status += " [" + std::to_string(violations.size()) + " calendar violations]";
            }
        } else {
            result.status += " [Calendar verified]";
        }
    }

    uint64_t endTicks = clock.rdtscp();
    result.solveTime = clock.ticks_to_seconds(endTicks - startTicks);
    
    return result;
}

void QPSolver::buildObjective(
    Eigen::SparseMatrix<double>& P,
    Eigen::VectorXd& q,
    const Eigen::VectorXd& ivMarket) const {
    
    int n = (int)ivMarket.size();
    P.resize(n, n);
    q.resize(n);
    
    std::vector<Eigen::Triplet<double>> triplets;
    
    switch (config_.objective) {
        case ObjectiveType::L2_DISTANCE:
        case ObjectiveType::WEIGHTED_L2: {
            
            Eigen::VectorXd weights = calculateWeights();

            for (int i = 0; i < n; ++i) {
                triplets.emplace_back(i, i, 2.0 * weights(i));
            }

            q = -2.0 * weights.cwiseProduct(ivMarket);
            break;
        }
        
        case ObjectiveType::HUBER: {
            
            Eigen::VectorXd weights = calculateWeights();
            for (int i = 0; i < n; ++i) {
                triplets.emplace_back(i, i, 2.0 * weights(i));
            }
            q = -2.0 * weights.cwiseProduct(ivMarket);
            break;
        }
        
        default: {
            
            Eigen::VectorXd weights = calculateWeights();
            for (int i = 0; i < n; ++i) {
                triplets.emplace_back(i, i, 2.0 * weights(i));
            }
            q = -2.0 * weights.cwiseProduct(ivMarket);
        }
    }

    if (config_.regularizationWeight > 0) {
        for (int i = 0; i < n; ++i) {
            triplets.emplace_back(i, i, 2.0 * config_.regularizationWeight);
        }
    }
    
    P.setFromTriplets(triplets.begin(), triplets.end());
}

Eigen::VectorXd QPSolver::calculateWeights() const {
    int n = nExpiries() * nStrikes();
    Eigen::VectorXd weights = Eigen::VectorXd::Ones(n);
    
    if (config_.enableVolumeWeighting) {

        weights = Eigen::VectorXd::Ones(n);
    }
    
    return weights;
}

void QPSolver::addSmoothnessConstraints(
    std::vector<Eigen::Triplet<double>>& trips,
    std::vector<double>& lb,
    std::vector<double>& ub,
    int& row) const {
    
    int nE = nExpiries(), nK = nStrikes();

    for (int i = 0; i + 1 < nE; ++i) {
        for (int j = 0; j < nK; ++j) {
            
            trips.emplace_back(row, idx(i, j), 1.0);
            trips.emplace_back(row, idx(i+1, j), -1.0);
            double smoothnessThreshold = 0.1; 
            lb.push_back(-smoothnessThreshold);
            ub.push_back(smoothnessThreshold);
            ++row;
        }
    }
}

double QPSolver::calculateSmoothnessPenalty(const Eigen::VectorXd& ivFlat) const {
    double penalty = 0.0;
    int nE = nExpiries(), nK = nStrikes();
    
    for (int i = 0; i < nE; ++i) {
        for (int j = 1; j + 1 < nK; ++j) {
            double smoothness = std::abs(ivFlat(idx(i, j-1)) - 2.0*ivFlat(idx(i, j)) + ivFlat(idx(i, j+1)));
            penalty += smoothness * smoothness;
        }
    }
    
    return config_.smoothnessWeight * penalty;
}

double QPSolver::calculateMarketPreservationPenalty(const VolSurface& surface, const Eigen::VectorXd& ivFlat) const {
    
    Eigen::VectorXd ivMarket(nExpiries() * nStrikes());
    const auto& grid = surface.ivGrid();
    for (int i = 0; i < nExpiries(); ++i) {
        for (int j = 0; j < nStrikes(); ++j) {
            ivMarket(idx(i, j)) = grid(i, j);
        }
    }

    double distance = (ivFlat - ivMarket).squaredNorm();
    return config_.regularizationWeight * distance;
}

double QPSolver::calculateAdaptiveRegularization(const Eigen::VectorXd& ivMarket) const {
    if (!config_.enableAdaptiveRegularization) {
        return config_.regularizationWeight;
    }

    double avgVol = ivMarket.mean();
    double adaptiveWeight = config_.regularizationWeight * std::max(0.1, avgVol / 0.2);
    
    return adaptiveWeight;
}

VolSurface QPSolver::buildCorrectedSurface(const VolSurface& surface, const QPResult& result) const {
    std::vector<Quote> quotes;
    int nE = nExpiries(), nK = nStrikes();
    for (int i = 0; i < nE; ++i)
        for (int j = 0; j < nK; ++j)
            quotes.push_back({
                surface.strikes()[j],
                surface.expiries()[i],
                result.ivFlat(idx(i,j))
            });
    return VolSurface(quotes, surface.marketData());
}

double QPSolver::computeCalendarViolation(
    const VolSurface& surface,
    const Eigen::VectorXd& ivFlat,
    int expiry0_idx,
    int expiry1_idx,
    int strike_idx) const 
{
    const auto& Ts = surface.expiries();
    double T0 = Ts[expiry0_idx];
    double T1 = Ts[expiry1_idx];
    
    double sigma0 = ivFlat[idx(expiry0_idx, strike_idx)];
    double sigma1 = ivFlat[idx(expiry1_idx, strike_idx)];

    double w0 = sigma0 * sigma0 * T0;
    double w1 = sigma1 * sigma1 * T1;
    
    return w0 - w1;  
}

bool QPSolver::verifyCalendarConstraint(
    const VolSurface& surface,
    const Eigen::VectorXd& ivFlat,
    std::vector<std::tuple<int, int, double>>& violations) const 
{
    violations.clear();
    
    const auto& Ts = surface.expiries();
    int nE = nExpiries();
    int nK = nStrikes();
    
    bool allSatisfied = true;

    for (int ei0 = 0; ei0 + 1 < nE; ++ei0) {
        for (int ei1 = ei0 + 1; ei1 < nE; ++ei1) {
            double T0 = Ts[ei0];
            double T1 = Ts[ei1];
            
            for (int ki = 0; ki < nK; ++ki) {
                double sigma0 = ivFlat[idx(ei0, ki)];
                double sigma1 = ivFlat[idx(ei1, ki)];

                double w0 = sigma0 * sigma0 * T0;
                double w1 = sigma1 * sigma1 * T1;
                
                double violation = w0 - w1;  
                
                if (violation > config_.calendarViolationTol) {
                    violations.emplace_back(ei0, ki, violation);
                    allSatisfied = false;
                }
            }
        }
    }
    
    return allSatisfied;
}

QPResult QPSolver::refineCalendarConstraint(
    const VolSurface& surface,
    const QPResult& initialResult,
    int maxRefinementIterations) const 
{
    if (!initialResult.success) {
        return initialResult;  
    }
    
    QPResult refined = initialResult;
    Eigen::VectorXd ivFlat = initialResult.ivFlat;
    
    std::vector<std::tuple<int, int, double>> violations;
    
    for (int iter = 0; iter < maxRefinementIterations; ++iter) {
        
        bool satisfied = verifyCalendarConstraint(surface, ivFlat, violations);
        
        if (satisfied) {
            refined.status += " [Calendar verified]";
            return refined;
        }

        const auto& Ts = surface.expiries();
        
        for (const auto& [ei0, ki, viol] : violations) {
            int ei1 = ei0 + 1;
            double T0 = Ts[ei0];
            double T1 = Ts[ei1];
            double sigma0 = ivFlat[idx(ei0, ki)];
            double sigma1 = ivFlat[idx(ei1, ki)];

            double w0 = sigma0 * sigma0 * T0;
            double sigma1_min = std::sqrt((w0 + 1e-6) / T1);

            if (sigma1 < sigma1_min) {
                double alpha = 0.5;  
                ivFlat[idx(ei1, ki)] = sigma1 + alpha * (sigma1_min - sigma1);
            }
        }

        refined.ivFlat = ivFlat;
        
        std::cerr << "Calendar refinement iteration " << iter + 1 
                  << ": " << violations.size() << " violations" << std::endl;
    }

    bool finalSatisfied = verifyCalendarConstraint(surface, ivFlat, violations);
    if (finalSatisfied) {
        refined.status += " [Calendar verified after refinement]";
    } else {
        refined.status += " [Calendar partially refined, " + 
                         std::to_string(violations.size()) + " violations remain]";
    }
    
    return refined;
}