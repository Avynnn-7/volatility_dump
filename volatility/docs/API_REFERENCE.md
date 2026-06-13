# API Reference

## Core Classes

### VolSurface

Bilinear interpolation volatility surface.

```cpp
class VolSurface {
public:
    // Construction
    explicit VolSurface(const std::vector<Quote>& quotes, 
                        const MarketData& marketData);
    
    // Interpolation
    double impliedVol(double strike, double expiry) const;
    double callPrice(double strike, double expiry) const;
    double putPrice(double strike, double expiry) const;
    
    // Accessors
    const std::vector<double>& strikes() const;
    const std::vector<double>& expiries() const;
    double spot() const;
    const MarketData& marketData() const;
    const Eigen::MatrixXd& ivGrid() const;
    
    // Financial calculations
    double forward(double expiry) const;
    double discountFactor(double expiry) const;
    
    // Cache management
    CacheStats getCacheStats() const;
    void clearCache();
    void setCacheSize(size_t maxEntries);
};
```

### ArbitrageDetector

Static arbitrage detection.

```cpp
class ArbitrageDetector {
public:
    explicit ArbitrageDetector(const VolSurface& surface);
    
    void setConfig(const Config& config);
    
    // Detection
    std::vector<ArbViolation> detect() const;
    std::vector<ArbViolation> checkButterfly() const;
    std::vector<ArbViolation> checkCalendar() const;
    std::vector<ArbViolation> checkMonotonicity() const;
    
    // Quality
    double getQualityScore() const;
    
    // Reporting
    static void report(const std::vector<ArbViolation>& violations);
};
```

### QPSolver

Quadratic programming solver.

```cpp
class QPSolver {
public:
    explicit QPSolver(const VolSurface& surface);
    QPSolver(const VolSurface& surface, const Config& config);
    
    void setConfig(const Config& config);
    const Config& getConfig() const;
    
    // Solving
    QPResult solve() const;
    VolSurface buildCorrectedSurface(const QPResult& result) const;
    
    // Constraints
    void buildConstraints(Eigen::SparseMatrix<double>& A,
                          Eigen::VectorXd& lb,
                          Eigen::VectorXd& ub) const;
    
    // Calendar refinement
    bool verifyCalendarConstraint(const Eigen::VectorXd& ivFlat,
                                  std::vector<std::tuple<int,int,double>>& violations) const;
    QPResult refineCalendarConstraint(const QPResult& initial,
                                      int maxIterations = 5) const;
};
```

### VolatilityArbitrageAPI

High-level API singleton.

```cpp
class VolatilityArbitrageAPI {
public:
    static VolatilityArbitrageAPI& getInstance();
    
    // Core operations
    ApiResponse checkArbitrage(const ArbitrageCheckRequest& request);
    ApiResponse correctSurface(const ArbitrageCheckRequest& request);
    ApiResponse analyzeQuality(const ArbitrageCheckRequest& request);
    
    // Batch operations
    ApiResponse batchCheckArbitrage(const std::vector<ArbitrageCheckRequest>& requests);
    
    // Real-time processing
    void startRealTimeProcessing(std::function<void(const ArbitrageCheckRequest&)> callback);
    void stopRealTimeProcessing();
    
    // Configuration
    ApiResponse updateConfiguration(const std::string& configJson);
    ApiResponse getConfiguration();
    
    // Status
    ApiResponse getStatus();
    ApiResponse getPerformanceMetrics();
    bool healthCheck();
    std::string getVersion();
};
```

## Data Structures

### Quote

```cpp
struct Quote {
    double strike;      // Strike price
    double expiry;      // Time to expiry (years)
    double iv;          // Implied volatility (decimal)
    double bid = 0.0;   // Bid IV (optional)
    double ask = 0.0;   // Ask IV (optional)
    double volume = 0.0; // Volume (optional)
};
```

### MarketData

```cpp
struct MarketData {
    double spot;            // Spot price
    double riskFreeRate;    // Risk-free rate
    double dividendYield;   // Dividend yield
    std::string valuationDate; // YYYY-MM-DD
    std::string currency;   // ISO 4217
};
```

### QPResult

```cpp
struct QPResult {
    bool success;              // Solver converged
    Eigen::VectorXd ivFlat;    // Corrected IV
    double objectiveValue;     // Final objective
    double regularizationPenalty;
    int iterations;
    std::string status;
    double solveTime;          // Seconds
};
```

### ArbViolation

```cpp
struct ArbViolation {
    ArbType type;           // Violation type
    double strike;          // Location
    double expiry;          // Location
    double magnitude;       // Severity
    double threshold;       // Detection threshold
    std::string description;
    
    double severityScore() const;
    bool isCritical() const;
};
```

## REST API Endpoints

### POST /api/v1/arbitrage/check

Check for arbitrage violations.

**Request:**
```json
{
  "quotes": [...],
  "marketData": {...},
  "interpolationMethod": "svi",
  "enableQPCorrection": true
}
```

**Response:**
```json
{
  "success": true,
  "arbitrageFree": false,
  "qualityScore": 0.85,
  "violations": [...],
  "correctionTime": 0.045
}
```

### GET /api/v1/status

Get system status.

**Response:**
```json
{
  "healthy": true,
  "version": "2.0.0",
  "uptime": 3600,
  "requestsProcessed": 1000
}
```

### POST /api/v1/config

Update configuration.

**Request:**
```json
{
  "qp.tolerance": 1e-12,
  "arbitrage.enable_parallelization": true
}
```

**Response:**
```json
{
  "success": true,
  "message": "Configuration updated"
}
```
