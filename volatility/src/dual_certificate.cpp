#include "dual_certificate.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

DualCertifier::DualCertifier(const Eigen::VectorXd& ivMarket,
                              const QPResult&        result,
                              const Eigen::SparseMatrix<double>& A,
                              const Eigen::VectorXd& lb,
                              const Eigen::VectorXd& ub)
    : ivMarket_(ivMarket), result_(result), A_(A), lb_(lb), ub_(ub) {}

DualCertificate DualCertifier::certify(double tol) const {
    DualCertificate cert;
    cert.valid  = false;

    if (!result_.success) {
        cert.summary = "QP did not solve to optimality  dual certificate unavailable.";
        cert.stationarityResidual = -1;
        cert.compSlackResidual    = -1;
        cert.dualFeasResidual     = -1;
        return cert;
    }

    const Eigen::VectorXd& x = result_.ivFlat;   
    int m = (int)lb_.size();

    Eigen::VectorXd rhs = ivMarket_ - x;
    
    Eigen::LeastSquaresConjugateGradient<Eigen::SparseMatrix<double>> lsq;
    lsq.compute(A_.transpose());
    cert.lambda = lsq.solve(rhs);

    Eigen::VectorXd stationarity = x - ivMarket_ + A_.transpose() * cert.lambda;
    cert.stationarityResidual = stationarity.norm();

    Eigen::VectorXd Ax = A_ * x;
    double csResid = 0.0;
    for (int i = 0; i < m; ++i) {
        
        double slack_lo = Ax(i) - lb_(i);
        double slack_hi = ub_(i) - Ax(i);
        
        double lam_i = (i < (int)cert.lambda.size()) ? cert.lambda(i) : 0.0;
        csResid += std::abs(lam_i * std::min(slack_lo, slack_hi));
    }
    cert.compSlackResidual = csResid;

    double dualViol = 0.0;
    for (int i = 0; i < (int)cert.lambda.size(); ++i)
        if (cert.lambda(i) < -tol) dualViol += std::abs(cert.lambda(i));
    cert.dualFeasResidual = dualViol;

    cert.valid = (cert.stationarityResidual < tol * 10)
              && (cert.dualFeasResidual      < tol * 10);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "KKT Certificate:\n";
    ss << "  Stationarity residual  : " << cert.stationarityResidual
       << (cert.stationarityResidual < tol*10 ? "  " : "  ") << "\n";
    ss << "  Comp. slackness resid  : " << cert.compSlackResidual    << "\n";
    ss << "  Dual feasibility viol  : " << cert.dualFeasResidual
       << (cert.dualFeasResidual < tol*10 ? "  " : "  ") << "\n";
    ss << "  Certificate valid      : " << (cert.valid ? "YES" : "NO") << "\n";

    if (cert.lambda.size() > 0) {
        std::vector<std::pair<double,int>> lams;
        for (int i = 0; i < (int)cert.lambda.size(); ++i)
            lams.push_back({std::abs(cert.lambda(i)), i});
        std::sort(lams.rbegin(), lams.rend());
        ss << "  Most active constraints (by ||):\n";
        for (int k = 0; k < std::min(3, (int)lams.size()); ++k)
            ss << "    constraint[" << lams[k].second << "]   = "
               << cert.lambda(lams[k].second) << "\n";
    }
    cert.summary = ss.str();
    return cert;
}

void DualCertifier::print(const DualCertificate& cert) const {
    std::cout << "\n=== Dual Certificate ===\n" << cert.summary << "\n";
}