

#include <iostream>
#include <iomanip>
#include "tsc_clock.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>

#include "vol_surface.hpp"
#include "arbitrage_detector.hpp"
#include "qp_solver.hpp"
#include "svi_surface.hpp"
#include "local_vol.hpp"
#include <nlohmann/json.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BlackScholes {

inline double normcdf(double x) {
    double a1 = 0.254829592;
    double a2 = -0.284496736;
    double a3 = 1.421413741;
    double a4 = -1.453152027;
    double a5 = 1.061405429;
    double p = 0.3275911;
    
    int sign = (x >= 0) ? 1 : -1;
    x = std::abs(x);
    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5 * t + a4) * t + a3) * t + a2) * t + a1) * t) * std::exp(-x * x / 2.0);
    return 0.5 * (1.0 + sign * (2.0 * y - 1.0));
}

inline double callPrice(double S, double K, double T, double sigma, double r, double q) {
    if (T <= 0.0 || sigma <= 0.0) {
        return std::max(S * std::exp(-q * T) - K * std::exp(-r * T), 0.0);
    }
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;
    return S * std::exp(-q * T) * normcdf(d1) - K * std::exp(-r * T) * normcdf(d2);
}

inline double putPrice(double S, double K, double T, double sigma, double r, double q) {
    if (T <= 0.0 || sigma <= 0.0) {
        return std::max(K * std::exp(-r * T) - S * std::exp(-q * T), 0.0);
    }
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;
    return K * std::exp(-r * T) * normcdf(-d2) - S * std::exp(-q * T) * normcdf(-d1);
}

inline double callDelta(double S, double K, double T, double sigma, double r, double q) {
    if (T <= 0.0 || sigma <= 0.0) return (S > K) ? 1.0 : 0.0;
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    return std::exp(-q * T) * normcdf(d1);
}

inline double gamma(double S, double K, double T, double sigma, double r, double q) {
    if (T <= 0.0 || sigma <= 0.0) return 0.0;
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double nprime_d1 = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * M_PI);
    return std::exp(-q * T) * nprime_d1 / (S * sigma * sqrtT);
}

inline double vega(double S, double K, double T, double sigma, double r, double q) {
    if (T <= 0.0 || sigma <= 0.0) return 0.0;
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double nprime_d1 = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * M_PI);
    return S * std::exp(-q * T) * nprime_d1 * sqrtT / 100.0;  
}

inline double impliedVol(double price, double S, double K, double T, double r, double q, bool isCall) {
    double sigma = 0.20;  
    for (int i = 0; i < 100; ++i) {
        double modelPrice = isCall ? callPrice(S, K, T, sigma, r, q) : putPrice(S, K, T, sigma, r, q);
        double v = vega(S, K, T, sigma, r, q);
        if (v < 1e-12) break;
        double diff = modelPrice - price;
        if (std::abs(diff) < 1e-8) break;
        sigma -= diff / (v * 100.0);  
        sigma = std::max(0.001, std::min(5.0, sigma));
    }
    return sigma;
}

}  

struct BenchmarkConfig {
    int warmupIterations = 10;
    int measureIterations = 100;
    bool verbose = false;
    bool jsonOutput = false;
    std::string outputFile;
};

struct BenchmarkResult {
    std::string name;
    std::string category;
    double meanTime;      
    double stdDev;        
    double minTime;
    double maxTime;
    double p50;           
    double p95;
    double p99;
    int iterations;
    double throughput;    
    
    nlohmann::json toJson() const {
        return {
            {"name", name},
            {"category", category},
            {"meanTime_us", meanTime},
            {"stdDev_us", stdDev},
            {"minTime_us", minTime},
            {"maxTime_us", maxTime},
            {"p50_us", p50},
            {"p95_us", p95},
            {"p99_us", p99},
            {"iterations", iterations},
            {"throughput_ops_sec", throughput}
        };
    }
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const BenchmarkConfig& config) : config_(config) {}
    
    template<typename Func>
    BenchmarkResult run(const std::string& name, const std::string& category, Func func) {
        std::vector<double> times;
        times.reserve(config_.measureIterations);
        arena::TscClock clock;

        for (int i = 0; i < config_.warmupIterations; ++i) {
            func();
        }

        for (int i = 0; i < config_.measureIterations; ++i) {
            uint64_t start = clock.rdtsc_fenced();
            func();
            uint64_t end = clock.rdtscp();
            
            double elapsed_us = clock.ticks_to_ns(end - start) / 1000.0;
            times.push_back(elapsed_us);
        }

        std::sort(times.begin(), times.end());
        
        BenchmarkResult result;
        result.name = name;
        result.category = category;
        result.iterations = config_.measureIterations;
        result.minTime = times.front();
        result.maxTime = times.back();

        double sum = std::accumulate(times.begin(), times.end(), 0.0);
        result.meanTime = sum / times.size();

        double sqSum = 0.0;
        for (double t : times) {
            sqSum += (t - result.meanTime) * (t - result.meanTime);
        }
        result.stdDev = std::sqrt(sqSum / times.size());

        result.p50 = times[times.size() / 2];
        result.p95 = times[static_cast<size_t>(times.size() * 0.95)];
        result.p99 = times[static_cast<size_t>(times.size() * 0.99)];

        result.throughput = 1e6 / result.meanTime;  
        
        return result;
    }
    
private:
    BenchmarkConfig config_;
};

std::vector<Quote> generateTestQuotes(int numStrikes, int numExpiries, double spot = 100.0) {
    std::vector<Quote> quotes;
    quotes.reserve(numStrikes * numExpiries);
    
    std::vector<double> expiries;
    for (int i = 1; i <= numExpiries; ++i) {
        expiries.push_back(i * 0.25);  
    }
    
    std::vector<double> strikes;
    double strikeStep = spot * 0.1;
    double minStrike = spot * 0.5;
    for (int i = 0; i < numStrikes; ++i) {
        strikes.push_back(minStrike + i * strikeStep);
    }

    for (double T : expiries) {
        for (double K : strikes) {
            double moneyness = std::log(K / spot);
            double atmVol = 0.20;
            double skew = -0.1 * moneyness;
            double smile = 0.05 * moneyness * moneyness;
            double iv = atmVol + skew + smile;
            iv = std::max(0.05, std::min(1.0, iv));  
            
            quotes.push_back({K, T, iv});
        }
    }
    
    return quotes;
}

MarketData getDefaultMarketData() {
    return {100.0, 0.05, 0.02, "2024-01-01", "USD"};
}

std::vector<BenchmarkResult> runBlackScholesBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;

    results.push_back(runner.run("BS_CallPrice_Single", "BlackScholes", []() {
        double price = BlackScholes::callPrice(100.0, 100.0, 0.25, 0.20, 0.05, 0.02);
        (void)price;  
    }));

    results.push_back(runner.run("BS_PutPrice_Single", "BlackScholes", []() {
        double price = BlackScholes::putPrice(100.0, 100.0, 0.25, 0.20, 0.05, 0.02);
        (void)price;
    }));

    results.push_back(runner.run("BS_CallDelta", "BlackScholes", []() {
        double delta = BlackScholes::callDelta(100.0, 100.0, 0.25, 0.20, 0.05, 0.02);
        (void)delta;
    }));

    results.push_back(runner.run("BS_Gamma", "BlackScholes", []() {
        double gamma = BlackScholes::gamma(100.0, 100.0, 0.25, 0.20, 0.05, 0.02);
        (void)gamma;
    }));

    results.push_back(runner.run("BS_Vega", "BlackScholes", []() {
        double vega = BlackScholes::vega(100.0, 100.0, 0.25, 0.20, 0.05, 0.02);
        (void)vega;
    }));

    results.push_back(runner.run("BS_ImpliedVol", "BlackScholes", []() {
        double iv = BlackScholes::impliedVol(10.45, 100.0, 100.0, 0.25, 0.05, 0.02, true);
        (void)iv;
    }));

    std::vector<double> spots(100, 100.0);
    std::vector<double> strikes(100), expiries(100), vols(100), rates(100), divs(100);
    for (int i = 0; i < 100; ++i) {
        strikes[i] = 80.0 + i * 0.4;
        expiries[i] = 0.25;
        vols[i] = 0.20;
        rates[i] = 0.05;
        divs[i] = 0.02;
    }
    
    results.push_back(runner.run("BS_BatchPricing_100", "BlackScholes", [&]() {
        for (int i = 0; i < 100; ++i) {
            double price = BlackScholes::callPrice(spots[i], strikes[i], expiries[i], 
                                                   vols[i], rates[i], divs[i]);
            (void)price;
        }
    }));
    
    return results;
}

std::vector<BenchmarkResult> runInterpolationBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;
    
    auto quotes = generateTestQuotes(10, 4);
    auto marketData = getDefaultMarketData();
    VolSurface surface(quotes, marketData);

    results.push_back(runner.run("Interp_BilinearSingle", "Interpolation", [&]() {
        double vol = surface.impliedVol(105.0, 0.5);
        (void)vol;
    }));

    results.push_back(runner.run("Interp_BilinearGrid_100", "Interpolation", [&]() {
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                double K = 80.0 + i * 4.0;
                double T = 0.1 + j * 0.1;
                double vol = surface.impliedVol(K, T);
                (void)vol;
            }
        }
    }));

    results.push_back(runner.run("SVI_Construction", "Interpolation", [&]() {
        SVISurface svi(quotes, marketData);
        (void)svi;
    }));

    SVISurface sviSurface(quotes, marketData);
    results.push_back(runner.run("Interp_SVISingle", "Interpolation", [&]() {
        double vol = sviSurface.impliedVol(105.0, 0.5);
        (void)vol;
    }));
    
    return results;
}

std::vector<BenchmarkResult> runArbitrageDetectionBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;

    {
        auto quotes = generateTestQuotes(5, 4);
        auto marketData = getDefaultMarketData();
        VolSurface surface(quotes, marketData);
        
        results.push_back(runner.run("ArbDetect_Small_20quotes", "ArbitrageDetection", [&]() {
            ArbitrageDetector detector(surface);
            auto violations = detector.detect();
            (void)violations;
        }));
    }

    {
        auto quotes = generateTestQuotes(10, 6);
        auto marketData = getDefaultMarketData();
        VolSurface surface(quotes, marketData);
        
        results.push_back(runner.run("ArbDetect_Medium_60quotes", "ArbitrageDetection", [&]() {
            ArbitrageDetector detector(surface);
            auto violations = detector.detect();
            (void)violations;
        }));
    }

    {
        auto quotes = generateTestQuotes(20, 10);
        auto marketData = getDefaultMarketData();
        VolSurface surface(quotes, marketData);
        
        results.push_back(runner.run("ArbDetect_Large_200quotes", "ArbitrageDetection", [&]() {
            ArbitrageDetector detector(surface);
            auto violations = detector.detect();
            (void)violations;
        }));
    }
    
    return results;
}

std::vector<BenchmarkResult> runQPSolverBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;

    {
        auto quotes = generateTestQuotes(5, 4);
        auto marketData = getDefaultMarketData();
        VolSurface surface(quotes, marketData);
        
        QPSolver solver;
        solver.setup(surface);
        results.push_back(runner.run("QPSolver_Small_20vars", "QPSolver", [&]() {
            auto result = solver.solve(surface);
            (void)result;
        }));
    }

    {
        auto quotes = generateTestQuotes(8, 5);
        auto marketData = getDefaultMarketData();
        VolSurface surface(quotes, marketData);
        
        QPSolver solver;
        solver.setup(surface);
        results.push_back(runner.run("QPSolver_Medium_40vars", "QPSolver", [&]() {
            auto result = solver.solve(surface);
            (void)result;
        }));
    }

    {
        auto quotes = generateTestQuotes(10, 6);
        auto marketData = getDefaultMarketData();
        VolSurface surface(quotes, marketData);
        
        BenchmarkConfig config;
        config.warmupIterations = 3;
        config.measureIterations = 20;
        BenchmarkRunner slowRunner(config);
        
        QPSolver solver;
        solver.setup(surface);
        results.push_back(slowRunner.run("QPSolver_Large_60vars", "QPSolver", [&]() {
            auto result = solver.solve(surface);
            (void)result;
        }));
    }
    
    return results;
}

std::vector<BenchmarkResult> runPipelineBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;
    
    auto quotes = generateTestQuotes(10, 5);
    auto marketData = getDefaultMarketData();

    QPSolver pipelineSolver;
    pipelineSolver.setup(VolSurface(quotes, marketData));
    
    results.push_back(runner.run("Pipeline_Full_DetectRepair", "Pipeline", [&]() {
        VolSurface surface(quotes, marketData);
        ArbitrageDetector detector(surface);
        auto violations = detector.detect();
        
        if (!violations.empty()) {
            auto result = pipelineSolver.solve(surface);
            (void)result;
        }
    }));

    results.push_back(runner.run("Pipeline_DetectionOnly", "Pipeline", [&]() {
        VolSurface surface(quotes, marketData);
        ArbitrageDetector detector(surface);
        auto violations = detector.detect();
        (void)violations;
    }));

    results.push_back(runner.run("Pipeline_LocalVol", "Pipeline", [&]() {
        VolSurface surface(quotes, marketData);
        LocalVolSurface localVol(surface);
        bool allPos = localVol.allPositive();
        (void)allPos;
    }));
    
    return results;
}

std::vector<BenchmarkResult> runScalabilityBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;
    
    auto marketData = getDefaultMarketData();

    for (int size : {20, 50, 100, 200}) {
        int numStrikes = static_cast<int>(std::sqrt(size));
        int numExpiries = size / numStrikes;
        auto quotes = generateTestQuotes(numStrikes, numExpiries);
        
        std::string name = "Scalability_" + std::to_string(size) + "quotes";
        
        results.push_back(runner.run(name, "Scalability", [&]() {
            VolSurface surface(quotes, marketData);
            ArbitrageDetector detector(surface);
            auto violations = detector.detect();
            (void)violations;
        }));
    }
    
    return results;
}

std::vector<BenchmarkResult> runMemoryBenchmarks(BenchmarkRunner& runner) {
    std::vector<BenchmarkResult> results;
    
    auto marketData = getDefaultMarketData();

    for (int size : {100, 500, 1000}) {
        int numStrikes = static_cast<int>(std::sqrt(size));
        int numExpiries = size / numStrikes;
        auto quotes = generateTestQuotes(numStrikes, numExpiries);
        
        std::string name = "Memory_SurfaceAlloc_" + std::to_string(size);
        
        results.push_back(runner.run(name, "Memory", [&]() {
            auto* surface = new VolSurface(quotes, marketData);
            delete surface;
        }));
    }
    
    return results;
}

void printResults(const std::vector<BenchmarkResult>& results, bool verbose) {
    std::string currentCategory;
    
    for (const auto& r : results) {
        if (r.category != currentCategory) {
            currentCategory = r.category;
            std::cout << "\n\n";
            std::cout << "  " << currentCategory << " Benchmarks\n";
            std::cout << "\n\n";
            std::cout << std::left << std::setw(35) << "Benchmark"
                      << std::right << std::setw(12) << "Mean (s)"
                      << std::setw(12) << "StdDev"
                      << std::setw(12) << "P50"
                      << std::setw(12) << "P95"
                      << std::setw(14) << "Throughput"
                      << "\n";
            std::cout << std::string(97, '-') << "\n";
        }
        
        std::cout << std::left << std::setw(35) << r.name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(12) << r.meanTime
                  << std::setw(12) << r.stdDev
                  << std::setw(12) << r.p50
                  << std::setw(12) << r.p95
                  << std::setw(12) << static_cast<int>(r.throughput) << " ops/s"
                  << "\n";
        
        if (verbose) {
            std::cout << "    Min: " << r.minTime << " s, Max: " << r.maxTime 
                      << " s, P99: " << r.p99 << " s\n";
        }
    }
}

void exportJsonResults(const std::vector<BenchmarkResult>& results, const std::string& filename) {
    nlohmann::json output;
    output["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    output["version"] = "1.0.0";
    
    nlohmann::json benchmarks = nlohmann::json::array();
    for (const auto& r : results) {
        benchmarks.push_back(r.toJson());
    }
    output["benchmarks"] = benchmarks;

    std::map<std::string, std::vector<double>> categoryTimes;
    for (const auto& r : results) {
        categoryTimes[r.category].push_back(r.meanTime);
    }
    
    nlohmann::json summary;
    for (const auto& [cat, times] : categoryTimes) {
        double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        summary[cat]["averageTime_us"] = avg;
        summary[cat]["benchmarkCount"] = times.size();
    }
    output["summary"] = summary;
    
    std::ofstream file(filename);
    if (file.is_open()) {
        file << output.dump(2);
        file.close();
        std::cout << "\nResults exported to: " << filename << "\n";
    } else {
        std::cerr << "Error: Could not write to file: " << filename << "\n";
    }
}

int main(int argc, char* argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            std::cout << "Volatility Arbitrage Benchmark Suite\n\n";
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --help, -h           Show this help message\n";
            std::cout << "  --verbose, -v        Show detailed output\n";
            std::cout << "  --iterations=N       Number of measurement iterations (default: 100)\n";
            std::cout << "  --warmup=N           Number of warmup iterations (default: 10)\n";
            std::cout << "  --output=FILE        Export results to JSON file\n";
            return 0;
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        } else if (arg.substr(0, 13) == "--iterations=") {
            config.measureIterations = std::stoi(arg.substr(13));
        } else if (arg.substr(0, 9) == "--warmup=") {
            config.warmupIterations = std::stoi(arg.substr(9));
        } else if (arg.substr(0, 9) == "--output=") {
            config.outputFile = arg.substr(9);
            config.jsonOutput = true;
        }
    }
    
    std::cout << "\n";
    std::cout << "  Volatility Arbitrage Performance Benchmark Suite\n";
    std::cout << "\n\n";
    std::cout << "Configuration:\n";
    std::cout << "  Warmup iterations:  " << config.warmupIterations << "\n";
    std::cout << "  Measure iterations: " << config.measureIterations << "\n";
    std::cout << "  Verbose output:     " << (config.verbose ? "yes" : "no") << "\n";
    if (config.jsonOutput) {
        std::cout << "  Output file:        " << config.outputFile << "\n";
    }
    std::cout << "\nRunning benchmarks...\n";
    
    BenchmarkRunner runner(config);
    std::vector<BenchmarkResult> allResults;

    auto bsResults = runBlackScholesBenchmarks(runner);
    allResults.insert(allResults.end(), bsResults.begin(), bsResults.end());
    
    auto interpResults = runInterpolationBenchmarks(runner);
    allResults.insert(allResults.end(), interpResults.begin(), interpResults.end());
    
    auto arbResults = runArbitrageDetectionBenchmarks(runner);
    allResults.insert(allResults.end(), arbResults.begin(), arbResults.end());
    
    auto qpResults = runQPSolverBenchmarks(runner);
    allResults.insert(allResults.end(), qpResults.begin(), qpResults.end());
    
    auto pipelineResults = runPipelineBenchmarks(runner);
    allResults.insert(allResults.end(), pipelineResults.begin(), pipelineResults.end());
    
    auto scaleResults = runScalabilityBenchmarks(runner);
    allResults.insert(allResults.end(), scaleResults.begin(), scaleResults.end());
    
    auto memResults = runMemoryBenchmarks(runner);
    allResults.insert(allResults.end(), memResults.begin(), memResults.end());

    printResults(allResults, config.verbose);

    if (config.jsonOutput) {
        exportJsonResults(allResults, config.outputFile);
    }
    
    std::cout << "\n\n";
    std::cout << "  Benchmark Complete - " << allResults.size() << " benchmarks executed\n";
    std::cout << "\n";
    
    return 0;
}
