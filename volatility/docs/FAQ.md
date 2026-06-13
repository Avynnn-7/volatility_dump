# Frequently Asked Questions

## General

### What is vol_arb?

vol_arb is a C++17 library for detecting and correcting static arbitrage in
implied volatility surfaces. It uses quadratic programming to project market
surfaces onto the closest arbitrage-free surface.

### What types of arbitrage does it detect?

1. **Butterfly** - Negative risk-neutral density (d²C/dK² < 0)
2. **Calendar** - Non-increasing total variance (d(σ²T)/dT < 0)
3. **Monotonicity** - Call prices increasing with strike (dC/dK > 0)
4. **Vertical Spread** - Spread value exceeding strike difference
5. **Extreme Values** - Unreasonable volatility levels
6. **Density Integrity** - Risk-neutral density not integrating to 1

### Is this suitable for production use?

Yes. Version 2.0 is designed for production with:
- Thread-safe operations
- Comprehensive error handling
- Performance optimizations (SIMD, OpenMP)
- Extensive testing

## Technical

### What are the dependencies?

- **Eigen3**: Linear algebra (required)
- **nlohmann/json**: JSON parsing (required)
- **OSQP**: QP solver (required)
- **OpenMP**: Parallelization (optional)

### Which compilers are supported?

- MSVC 2019+ (Windows)
- GCC 9+ (Linux)
- Clang 10+ (Linux/macOS)

### How accurate is the QP correction?

The QP solver finds the closest arbitrage-free surface in L² distance.
Default tolerance is 1e-9. The dual certificate verifies optimality.

### Why does the QP solver fail to converge?

Common causes:
1. **Infeasible constraints**: Market data may be too far from arbitrage-free
2. **Numerical issues**: Try increasing `regularizationWeight`
3. **Iteration limit**: Increase `maxIterations`
4. **Tolerance too tight**: Relax `tolerance` to 1e-6

### What's the difference between VolSurface and SVISurface?

- **VolSurface**: Bilinear interpolation on a grid (fast, simple)
- **SVISurface**: SVI parameterization (smooth, fewer parameters, industry standard)

## Performance

### How fast is arbitrage detection?

Typical timings on modern hardware:
- 100 quotes: < 1ms
- 1,000 quotes: ~5ms
- 10,000 quotes: ~50ms (with OpenMP)

### How can I improve performance?

1. Enable OpenMP parallelization
2. Build with `-O3 -mavx2` (GCC/Clang)
3. Use the LRU cache (enabled by default)
4. Batch operations with `batchCheckArbitrage()`

### Does it support GPU acceleration?

Not currently. GPU support is on the roadmap for v2.1.

## Data

### What data formats are supported?

- JSON (recommended)
- CSV

### Can I use Bloomberg/Reuters data?

The DataFeed interface provides stubs for Bloomberg and Reuters.
Actual integration requires their proprietary SDKs.

### How should I handle missing data?

The DataHandler can fill missing data using interpolation.
Enable with `config.enableDataCleaning = true`.

## Troubleshooting

### "OSQP: Problem is infeasible"

The market surface is too far from any arbitrage-free surface.
Try:
1. Remove obvious outliers
2. Relax bounds (`minVol = 0.001`, `maxVol = 5.0`)
3. Check data quality metrics

### "NaN in local volatility"

This indicates d²C/dK² <= 0 (butterfly arbitrage).
Run QP correction first to ensure an arbitrage-free surface.

### Build errors with OSQP

Ensure you're using OSQP v1.x (not v0.6.x).
The API changed significantly between versions.

### Memory usage is high

1. Reduce cache size: `surface.setCacheSize(1000)`
2. Clear cache between operations: `surface.clearCache()`
3. Use memory pool for temporary allocations

## API

### Is there a Python wrapper?

Not yet. A pybind11 wrapper is planned for future releases.

### Can I use this in a web service?

Yes. Use the RestAPIHandler class with your preferred HTTP framework
(cpp-httplib, Crow, etc.).

### Is it thread-safe?

Yes. The main API classes use mutexes for thread safety.
VolSurface uses shared_mutex for concurrent reads.
