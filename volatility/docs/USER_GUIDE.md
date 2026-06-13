# User Guide

## Quick Start

### Installation

1. **Prerequisites**
   ```bash
   # Install vcpkg
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg && ./bootstrap-vcpkg.bat  # Windows
   cd vcpkg && ./bootstrap-vcpkg.sh   # Linux/macOS
   
   # Install dependencies
   vcpkg install eigen3 nlohmann-json osqp
   ```

2. **Build**
   ```powershell
   # Windows
   mkdir build && cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -A x64
   cmake --build . --config Release
   ```

3. **Run**
   ```powershell
   .\Release\vol_arb.exe data\sample_quotes.json
   ```

## Basic Usage

### Loading Market Data

```cpp
#include "data_handler.hpp"

// Configure data loading
DataHandler::Config config;
config.source = DataSource::JSON_FILE;
config.filePath = "market_data.json";
config.enableOutlierDetection = true;

// Load and validate
DataHandler handler(config);
auto [quotes, marketData] = handler.loadData();

// Check data quality
const auto& metrics = handler.getQualityMetrics();
if (!metrics.isAcceptable()) {
    std::cerr << "Data quality issues detected" << std::endl;
}
```

### Creating a Volatility Surface

```cpp
#include "vol_surface.hpp"

// Create surface from quotes
VolSurface surface(quotes, marketData);

// Query implied volatility
double iv = surface.impliedVol(100.0, 0.25);  // K=100, T=0.25yr
std::cout << "IV: " << (iv * 100) << "%" << std::endl;

// Get call price
double price = surface.callPrice(100.0, 0.25);
std::cout << "Call price: " << price << std::endl;
```

### Detecting Arbitrage

```cpp
#include "arbitrage_detector.hpp"

// Create detector
ArbitrageDetector detector(surface);

// Configure thresholds
ArbitrageDetector::Config config;
config.butterflyThreshold = 1e-6;
config.enableParallelization = true;
detector.setConfig(config);

// Run detection
auto violations = detector.detect();

// Report results
if (violations.empty()) {
    std::cout << "Surface is arbitrage-free!" << std::endl;
} else {
    ArbitrageDetector::report(violations);
}

// Get quality score
double quality = detector.getQualityScore();
std::cout << "Quality: " << (quality * 100) << "%" << std::endl;
```

### Correcting Arbitrage

```cpp
#include "qp_solver.hpp"
#include "dual_certificate.hpp"

// Create QP solver
QPSolver solver(surface);

// Configure
QPSolver::Config config;
config.tolerance = 1e-9;
config.maxIterations = 10000;
solver.setConfig(config);

// Solve
QPResult result = solver.solve();

if (result.success) {
    std::cout << "QP solved in " << result.solveTime << "s" << std::endl;
    std::cout << "Objective: " << result.objectiveValue << std::endl;
    
    // Build corrected surface
    VolSurface corrected = solver.buildCorrectedSurface(result);
    
    // Verify with dual certificate
    Eigen::SparseMatrix<double> A;
    Eigen::VectorXd lb, ub;
    solver.buildConstraints(A, lb, ub);
    
    DualCertifier certifier(ivMarket, result, A, lb, ub);
    auto cert = certifier.certify();
    
    if (cert.valid) {
        std::cout << "Solution is provably optimal" << std::endl;
    }
}
```

### Using the High-Level API

```cpp
#include "vol_api.hpp"

// Get API singleton
auto& api = VolatilityArbitrageAPI::getInstance();

// Create request
ArbitrageCheckRequest request;
request.quotes = quotes;
request.marketData = marketData;
request.interpolationMethod = "svi";  // or "bilinear"
request.enableQPCorrection = true;

// Process
ApiResponse response = api.checkArbitrage(request);

if (response.success) {
    std::cout << "Result: " << response.data << std::endl;
} else {
    std::cerr << "Error: " << response.message << std::endl;
}
```

## Data Formats

### JSON Input Format

```json
{
  "spot": 100.0,
  "riskFreeRate": 0.05,
  "dividendYield": 0.02,
  "valuationDate": "2024-01-15",
  "currency": "USD",
  "quotes": [
    {
      "strike": 95.0,
      "expiry": 0.25,
      "iv": 0.22,
      "bid": 0.21,
      "ask": 0.23,
      "volume": 1500
    },
    {
      "strike": 100.0,
      "expiry": 0.25,
      "iv": 0.20,
      "bid": 0.19,
      "ask": 0.21,
      "volume": 5000
    }
  ]
}
```

### CSV Input Format

```csv
strike,expiry,iv,bid,ask,volume
95.0,0.25,0.22,0.21,0.23,1500
100.0,0.25,0.20,0.19,0.21,5000
105.0,0.25,0.21,0.20,0.22,2000
```

## Configuration

### Configuration File (config.json)

```json
{
  "qp": {
    "tolerance": 1e-9,
    "max_iterations": 10000,
    "regularization_weight": 1e-6,
    "min_vol": 0.001,
    "max_vol": 5.0
  },
  "arbitrage": {
    "butterfly_threshold": 1e-6,
    "calendar_threshold": 1e-6,
    "enable_parallelization": true
  },
  "data": {
    "outlier_threshold": 3.0,
    "min_vol": 0.01,
    "max_vol": 3.0,
    "enable_cleaning": true
  },
  "log": {
    "level": 1,
    "file": "vol_arb.log",
    "console": true
  }
}
```

### Loading Configuration

```cpp
#include "config_manager.hpp"

auto& config = ConfigManager::getInstance();
config.loadFromFile("config.json");

// Access values
double tol = config.get<double>("qp.tolerance", 1e-9);
int maxIter = config.get<int>("qp.max_iterations", 10000);

// Modify and save
config.set("qp.verbose", true);
config.saveToFile("config.json");
```

## Performance Tips

1. **Enable OpenMP**: Build with OpenMP for parallel detection
2. **Use Caching**: Surface queries are cached automatically
3. **Batch Processing**: Use `batchCheckArbitrage()` for multiple surfaces
4. **Memory Pools**: Use `ThreadLocalPool` in tight loops

## Troubleshooting

### QP Solver Not Converging

1. Increase `maxIterations`
2. Relax `tolerance` (try 1e-6)
3. Enable `verbose` mode to see iteration progress
4. Check for extreme market data values

### High Memory Usage

1. Reduce cache size: `surface.setCacheSize(1000)`
2. Use `clearCache()` between operations
3. Enable memory pool for temporary allocations

### Slow Performance

1. Enable OpenMP parallelization
2. Use SIMD-optimized builds (`-mavx2` on GCC/Clang)
3. Reduce detection thresholds (fewer violations to check)
