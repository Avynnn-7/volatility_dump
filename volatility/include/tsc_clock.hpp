#pragma once

#include <cstdint>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace arena {

// getting time from normal clock calls OS kernels which takes too much time around 100 to 300 ns, instead directly read cycles straight from the cpu around 10ns
// rdtscp forces instruction serialization unlike rdtsc

class TscClock {
public:
    TscClock() {
        calibrate();
    }

    static inline uint64_t rdtscp() {
        unsigned int aux;
        return __rdtscp(&aux);
    }

    static inline uint64_t rdtscp(unsigned int& core_id) {
        return __rdtscp(&core_id);
    }

    static inline uint64_t rdtsc_fenced() {
        _mm_lfence();
        return __rdtsc();
    }

    double ticks_to_ns(uint64_t ticks) const {
        return static_cast<double>(ticks) / ticks_per_ns_;
    }

    double ticks_to_seconds(uint64_t ticks) const {
        return static_cast<double>(ticks) / (ticks_per_ns_ * 1e9);
    }

    double get_ticks_per_ns() const { return ticks_per_ns_; }
    double get_estimated_freq_ghz() const { return ticks_per_ns_; }

private:
    double ticks_per_ns_;
    void calibrate() {
        constexpr int calibration_rounds = 5;
        constexpr auto calibration_duration = std::chrono::milliseconds(50);
        double measurements[calibration_rounds];

        for (int i = 0; i < calibration_rounds; ++i) {
            auto wall_start = std::chrono::steady_clock::now();
            uint64_t tsc_start = rdtscp();

            std::this_thread::sleep_for(calibration_duration);

            uint64_t tsc_end = rdtscp();
            auto wall_end = std::chrono::steady_clock::now();

            double wall_ns = std::chrono::duration<double, std::nano>(wall_end - wall_start).count();
            double tsc_delta = static_cast<double>(tsc_end - tsc_start);
            measurements[i] = tsc_delta / wall_ns;
        }

        for (int i = 0; i < calibration_rounds - 1; ++i) {
            for (int j = i + 1; j < calibration_rounds; ++j) {
                if (measurements[j] < measurements[i]) {
                    double tmp = measurements[i];
                    measurements[i] = measurements[j];
                    measurements[j] = tmp;
                }
            }
        }
        ticks_per_ns_ = measurements[calibration_rounds / 2]; 
        // we take median of a few sleeps for preventing OS Switch Bia0 , thats why divided by 2 to get the middle element of sorted array i.e the median element
    }
};

} // namespace arena
