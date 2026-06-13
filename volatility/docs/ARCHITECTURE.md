# Architecture Overview

## System Architecture

vol_arb follows a layered architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            API LAYER                                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
│  │ VolatilityArb   │  │ RestAPIHandler  │  │ AsyncTask       │             │
│  │ itrageAPI       │  │                 │  │ Processor       │             │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘             │
└───────────┼─────────────────────┼─────────────────────┼─────────────────────┘
            │                     │                     │
            ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CORE LAYER                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │ VolSurface  │  │ SVISurface  │  │ Arbitrage   │  │ QPSolver    │        │
│  │             │  │             │  │ Detector    │  │             │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         │                │                │                │               │
│         ▼                ▼                ▼                ▼               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │ LocalVol    │  │ SVICalibr   │  │ DualCertif  │  │ OSQP        │        │
│  │ Surface     │  │ ator        │  │ ier         │  │ (external)  │        │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
            │                     │                     │
            ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       INFRASTRUCTURE LAYER                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │ DataHandler │  │ ConfigMgr   │  │ Logger      │  │ Validator   │        │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
            │                     │                     │
            ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       PERFORMANCE LAYER                                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                         │
│  │ MemoryPool  │  │ SIMD Math   │  │ Optimization│                         │
│  │             │  │             │  │ Hints       │                         │
│  └─────────────┘  └─────────────┘  └─────────────┘                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Component Descriptions

### API Layer

| Component | Responsibility |
|-----------|----------------|
| `VolatilityArbitrageAPI` | Singleton entry point for all operations |
| `RestAPIHandler` | HTTP endpoint handlers for web integration |
| `AsyncTaskProcessor` | Async task execution and batching |

### Core Layer

| Component | Responsibility |
|-----------|----------------|
| `VolSurface` | Bilinear interpolation volatility surface |
| `SVISurface` | SVI-parameterized volatility surface |
| `ArbitrageDetector` | Static arbitrage violation detection |
| `QPSolver` | QP-based arbitrage correction |
| `LocalVolSurface` | Dupire local volatility computation |
| `DualCertifier` | KKT optimality verification |

### Infrastructure Layer

| Component | Responsibility |
|-----------|----------------|
| `DataHandler` | Data loading, validation, cleaning |
| `ConfigManager` | JSON-based configuration management |
| `Logger` | Thread-safe logging system |
| `Validator` | Input validation framework |

### Performance Layer

| Component | Responsibility |
|-----------|----------------|
| `MemoryPool` | Fast allocation with bulk deallocation |
| `simd::*` | AVX/SSE vectorized math functions |
| `vol_opt::*` | Branch prediction and cache hints |

## Data Flow

### Arbitrage Check Flow

```
1. User Request
       │
       ▼
2. VolatilityArbitrageAPI::checkArbitrage()
       │
       ├─────────────────────────────────────┐
       ▼                                     ▼
3. DataHandler::loadData()          3. (if provided directly)
       │                                     │
       ▼                                     │
4. Validation & Cleaning                     │
       │                                     │
       └─────────────────┬───────────────────┘
                         ▼
5. VolSurface/SVISurface construction
                         │
                         ▼
6. ArbitrageDetector::detect()
                         │
                         ├──── No violations ──▶ Return quality score
                         │
                         ▼
7. QPSolver::solve()
                         │
                         ▼
8. DualCertifier::certify()
                         │
                         ▼
9. Return corrected surface + certificate
```

## Threading Model

- **Read Operations**: Multiple readers via `std::shared_mutex`
- **Write Operations**: Exclusive locks for modifications
- **Batch Processing**: OpenMP parallelization for detection
- **Real-time Queue**: Producer-consumer pattern with condition variables

## Memory Management

- **Surface Data**: Eigen matrices (contiguous storage)
- **Cache**: LRU eviction with configurable size
- **Temporary Objects**: Memory pool with bulk deallocation
- **Thread-local**: Separate pools per OpenMP thread

## External Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| Eigen3 | 3.4+ | Linear algebra |
| nlohmann/json | 3.11+ | JSON parsing |
| OSQP | 1.0+ | QP solving |
| OpenMP | 4.5+ | Parallelization (optional) |

## File Organization

```
vol_arb/
├── include/           # Header files (public API)
│   ├── vol_surface.hpp
│   ├── arbitrage_detector.hpp
│   └── ...
├── src/               # Implementation files
│   ├── vol_surface.cpp
│   └── ...
├── tests/             # Test files
├── data/              # Sample data files
├── docs/              # Documentation
└── CMakeLists.txt     # Build configuration
```
