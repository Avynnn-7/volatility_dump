

#pragma once
#include "vol_surface.hpp"
#include <Eigen/Dense>

class LocalVolSurface {
public:
    
    explicit LocalVolSurface(const VolSurface& surface);

    double localVol(double strike, double expiry) const;

    const Eigen::MatrixXd& localVolGrid() const { return lvGrid_; }

    void print() const;

    bool allPositive() const;

private:
    const VolSurface& surface_;   
    Eigen::MatrixXd lvGrid_;      

    void buildGrid();             

    double dCdT(double K, double T) const;    
    double d2CdK2(double K, double T) const;  
};
