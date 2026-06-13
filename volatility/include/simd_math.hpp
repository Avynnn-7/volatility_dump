

#pragma once

#include <cmath>
#include <cstddef>

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #include <immintrin.h>
#endif

namespace simd {

#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define SIMD_AVX2_ENABLED 1
#elif defined(__AVX__) || (defined(_MSC_VER) && defined(__AVX__))
    #define SIMD_AVX_ENABLED 1
#elif defined(__SSE4_2__) || (defined(_MSC_VER) && defined(__SSE4_2__))
    #define SIMD_SSE42_ENABLED 1
#else
    #define SIMD_SCALAR_ONLY 1
#endif

inline double normcdf(double x) {
    const double a1 =  0.254829592;
    const double a2 = -0.284496736;
    const double a3 =  1.421413741;
    const double a4 = -1.453152027;
    const double a5 =  1.061405429;
    const double p  =  0.3275911;
    
    int sign = (x < 0) ? -1 : 1;
    x = std::fabs(x);
    
    double t = 1.0 / (1.0 + p * x);
    double t2 = t * t;
    double t3 = t2 * t;
    double t4 = t3 * t;
    double t5 = t4 * t;
    
    double y = 1.0 - (a1*t + a2*t2 + a3*t3 + a4*t4 + a5*t5) * std::exp(-x*x/2.0);
    
    return 0.5 * (1.0 + sign * (2.0 * y - 1.0));
}

inline double blackScholesCallScalar(
    double S, double K, double T, 
    double r, double q, double sigma) 
{
    if (T <= 0.0 || sigma <= 0.0) {
        return std::max(S * std::exp(-q * T) - K * std::exp(-r * T), 0.0);
    }
    
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;
    
    double Nd1 = normcdf(d1);
    double Nd2 = normcdf(d2);
    
    return S * std::exp(-q * T) * Nd1 - K * std::exp(-r * T) * Nd2;
}

inline double blackScholesPutScalar(
    double S, double K, double T, 
    double r, double q, double sigma) 
{
    if (T <= 0.0 || sigma <= 0.0) {
        return std::max(K * std::exp(-r * T) - S * std::exp(-q * T), 0.0);
    }
    
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;
    
    double Nd1 = normcdf(-d1);
    double Nd2 = normcdf(-d2);
    
    return K * std::exp(-r * T) * Nd2 - S * std::exp(-q * T) * Nd1;
}

#if defined(SIMD_AVX2_ENABLED) || defined(SIMD_AVX_ENABLED)

inline __m256d exp_approx_avx(__m256d x) {
    
    const __m256d max_val = _mm256_set1_pd(709.0);   
    const __m256d min_val = _mm256_set1_pd(-709.0);
    x = _mm256_min_pd(_mm256_max_pd(x, min_val), max_val);

    const __m256d log2e = _mm256_set1_pd(1.4426950408889634);
    const __m256d one = _mm256_set1_pd(1.0);

    const __m256d c0 = _mm256_set1_pd(1.0);
    const __m256d c1 = _mm256_set1_pd(0.6931471805599453);   
    const __m256d c2 = _mm256_set1_pd(0.2402265069591007);
    const __m256d c3 = _mm256_set1_pd(0.0555041086648216);
    const __m256d c4 = _mm256_set1_pd(0.0096181291076285);
    const __m256d c5 = _mm256_set1_pd(0.0013333558146428);
    
    __m256d t = _mm256_mul_pd(x, log2e);

    __m256d ti = _mm256_round_pd(t, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    __m256d tf = _mm256_sub_pd(t, ti);

    __m256d p = c5;
    p = _mm256_add_pd(_mm256_mul_pd(p, tf), c4);
    p = _mm256_add_pd(_mm256_mul_pd(p, tf), c3);
    p = _mm256_add_pd(_mm256_mul_pd(p, tf), c2);
    p = _mm256_add_pd(_mm256_mul_pd(p, tf), c1);
    p = _mm256_add_pd(_mm256_mul_pd(p, tf), c0);

    __m128i ti_int = _mm256_cvtpd_epi32(ti);
    __m256i ti_64 = _mm256_cvtepi32_epi64(ti_int);
    __m256i bias = _mm256_set1_epi64x(1023);
    __m256i exp_bits = _mm256_slli_epi64(_mm256_add_epi64(ti_64, bias), 52);
    __m256d scale = _mm256_castsi256_pd(exp_bits);
    
    return _mm256_mul_pd(p, scale);
}

inline __m256d log_approx_avx(__m256d x) {
    
    const __m256d ln2 = _mm256_set1_pd(0.6931471805599453);
    const __m256d one = _mm256_set1_pd(1.0);

    __m256i xi = _mm256_castpd_si256(x);
    __m256i exp_mask = _mm256_set1_epi64x(0x7FF0000000000000ULL);
    __m256i mantissa_mask = _mm256_set1_epi64x(0x000FFFFFFFFFFFFFULL);
    __m256i bias = _mm256_set1_epi64x(1023);

    __m256i exp_bits = _mm256_srli_epi64(_mm256_and_si256(xi, exp_mask), 52);
    __m256d exp_d = _mm256_cvtepi32_pd(_mm256_castsi256_si128(
        _mm256_permute4x64_epi64(_mm256_sub_epi64(exp_bits, bias), 0xD8)));

    __m256i one_bits = _mm256_set1_epi64x(0x3FF0000000000000ULL);
    __m256i mantissa_bits = _mm256_or_si256(_mm256_and_si256(xi, mantissa_mask), one_bits);
    __m256d m = _mm256_castsi256_pd(mantissa_bits);

    __m256d t = _mm256_sub_pd(m, one);
    
    const __m256d c1 = _mm256_set1_pd(1.0);
    const __m256d c2 = _mm256_set1_pd(-0.5);
    const __m256d c3 = _mm256_set1_pd(0.333333333333333);
    const __m256d c4 = _mm256_set1_pd(-0.25);
    const __m256d c5 = _mm256_set1_pd(0.2);
    
    __m256d p = c5;
    p = _mm256_add_pd(_mm256_mul_pd(p, t), c4);
    p = _mm256_add_pd(_mm256_mul_pd(p, t), c3);
    p = _mm256_add_pd(_mm256_mul_pd(p, t), c2);
    p = _mm256_add_pd(_mm256_mul_pd(p, t), c1);
    p = _mm256_mul_pd(p, t);

    return _mm256_add_pd(_mm256_mul_pd(exp_d, ln2), p);
}

inline __m256d normcdf_approx_avx(__m256d x) {
    const __m256d a1 = _mm256_set1_pd(0.254829592);
    const __m256d a2 = _mm256_set1_pd(-0.284496736);
    const __m256d a3 = _mm256_set1_pd(1.421413741);
    const __m256d a4 = _mm256_set1_pd(-1.453152027);
    const __m256d a5 = _mm256_set1_pd(1.061405429);
    const __m256d p = _mm256_set1_pd(0.3275911);
    const __m256d one = _mm256_set1_pd(1.0);
    const __m256d half = _mm256_set1_pd(0.5);
    const __m256d neg_half = _mm256_set1_pd(-0.5);
    const __m256d sign_mask = _mm256_set1_pd(-0.0);

    __m256d abs_x = _mm256_andnot_pd(sign_mask, x);

    __m256d t = _mm256_div_pd(one, _mm256_add_pd(one, _mm256_mul_pd(p, abs_x)));

    __m256d poly = a5;
    poly = _mm256_add_pd(_mm256_mul_pd(poly, t), a4);
    poly = _mm256_add_pd(_mm256_mul_pd(poly, t), a3);
    poly = _mm256_add_pd(_mm256_mul_pd(poly, t), a2);
    poly = _mm256_add_pd(_mm256_mul_pd(poly, t), a1);
    poly = _mm256_mul_pd(poly, t);

    __m256d x2 = _mm256_mul_pd(x, x);
    __m256d exp_arg = _mm256_mul_pd(neg_half, x2);
    __m256d exp_val = exp_approx_avx(exp_arg);

    __m256d result_pos = _mm256_sub_pd(one, _mm256_mul_pd(poly, exp_val));

    __m256d zero = _mm256_setzero_pd();
    __m256d cmp_mask = _mm256_cmp_pd(x, zero, _CMP_LT_OQ);
    __m256d result_neg = _mm256_sub_pd(one, result_pos);
    
    return _mm256_blendv_pd(result_pos, result_neg, cmp_mask);
}

inline __m256d blackscholes_call_avx(
    __m256d S, __m256d K, __m256d T, 
    __m256d r, __m256d q, __m256d sigma) 
{
    __m256d one = _mm256_set1_pd(1.0);
    __m256d half = _mm256_set1_pd(0.5);
    __m256d zero = _mm256_setzero_pd();

    __m256d sqrt_T = _mm256_sqrt_pd(T);
    __m256d sigma_sqrt_T = _mm256_mul_pd(sigma, sqrt_T);

    __m256d log_SK = log_approx_avx(_mm256_div_pd(S, K));

    __m256d sigma2 = _mm256_mul_pd(sigma, sigma);
    __m256d drift = _mm256_add_pd(_mm256_sub_pd(r, q), _mm256_mul_pd(half, sigma2));
    drift = _mm256_mul_pd(drift, T);

    __m256d d1 = _mm256_div_pd(_mm256_add_pd(log_SK, drift), sigma_sqrt_T);

    __m256d d2 = _mm256_sub_pd(d1, sigma_sqrt_T);

    __m256d Nd1 = normcdf_approx_avx(d1);
    __m256d Nd2 = normcdf_approx_avx(d2);

    __m256d neg_qT = _mm256_sub_pd(zero, _mm256_mul_pd(q, T));
    __m256d term1 = _mm256_mul_pd(S, exp_approx_avx(neg_qT));
    term1 = _mm256_mul_pd(term1, Nd1);

    __m256d neg_rT = _mm256_sub_pd(zero, _mm256_mul_pd(r, T));
    __m256d term2 = _mm256_mul_pd(K, exp_approx_avx(neg_rT));
    term2 = _mm256_mul_pd(term2, Nd2);

    return _mm256_sub_pd(term1, term2);
}

inline __m256d blackscholes_put_avx(
    __m256d S, __m256d K, __m256d T, 
    __m256d r, __m256d q, __m256d sigma) 
{
    __m256d one = _mm256_set1_pd(1.0);
    __m256d half = _mm256_set1_pd(0.5);
    __m256d zero = _mm256_setzero_pd();
    
    __m256d sqrt_T = _mm256_sqrt_pd(T);
    __m256d sigma_sqrt_T = _mm256_mul_pd(sigma, sqrt_T);
    
    __m256d log_SK = log_approx_avx(_mm256_div_pd(S, K));
    
    __m256d sigma2 = _mm256_mul_pd(sigma, sigma);
    __m256d drift = _mm256_add_pd(_mm256_sub_pd(r, q), _mm256_mul_pd(half, sigma2));
    drift = _mm256_mul_pd(drift, T);
    
    __m256d d1 = _mm256_div_pd(_mm256_add_pd(log_SK, drift), sigma_sqrt_T);
    __m256d d2 = _mm256_sub_pd(d1, sigma_sqrt_T);

    __m256d neg_d1 = _mm256_sub_pd(zero, d1);
    __m256d neg_d2 = _mm256_sub_pd(zero, d2);
    __m256d Nnd1 = normcdf_approx_avx(neg_d1);
    __m256d Nnd2 = normcdf_approx_avx(neg_d2);

    __m256d neg_rT = _mm256_sub_pd(zero, _mm256_mul_pd(r, T));
    __m256d term1 = _mm256_mul_pd(K, exp_approx_avx(neg_rT));
    term1 = _mm256_mul_pd(term1, Nnd2);

    __m256d neg_qT = _mm256_sub_pd(zero, _mm256_mul_pd(q, T));
    __m256d term2 = _mm256_mul_pd(S, exp_approx_avx(neg_qT));
    term2 = _mm256_mul_pd(term2, Nnd1);

    return _mm256_sub_pd(term1, term2);
}

#endif 

inline void batchCallPrices(
    const double* S, const double* K, const double* T,
    double r, double q, const double* sigma,
    double* prices, int n)
{
#if defined(SIMD_AVX2_ENABLED) || defined(SIMD_AVX_ENABLED)
    __m256d r_vec = _mm256_set1_pd(r);
    __m256d q_vec = _mm256_set1_pd(q);
    
    int i = 0;
    
    for (; i + 4 <= n; i += 4) {
        __m256d S_vec = _mm256_loadu_pd(&S[i]);
        __m256d K_vec = _mm256_loadu_pd(&K[i]);
        __m256d T_vec = _mm256_loadu_pd(&T[i]);
        __m256d sigma_vec = _mm256_loadu_pd(&sigma[i]);
        
        __m256d result = blackscholes_call_avx(
            S_vec, K_vec, T_vec, r_vec, q_vec, sigma_vec);
        
        _mm256_storeu_pd(&prices[i], result);
    }

    for (; i < n; ++i) {
        prices[i] = blackScholesCallScalar(S[i], K[i], T[i], r, q, sigma[i]);
    }
#else
    
    for (int i = 0; i < n; ++i) {
        prices[i] = blackScholesCallScalar(S[i], K[i], T[i], r, q, sigma[i]);
    }
#endif
}

inline void batchPutPrices(
    const double* S, const double* K, const double* T,
    double r, double q, const double* sigma,
    double* prices, int n)
{
#if defined(SIMD_AVX2_ENABLED) || defined(SIMD_AVX_ENABLED)
    __m256d r_vec = _mm256_set1_pd(r);
    __m256d q_vec = _mm256_set1_pd(q);
    
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d S_vec = _mm256_loadu_pd(&S[i]);
        __m256d K_vec = _mm256_loadu_pd(&K[i]);
        __m256d T_vec = _mm256_loadu_pd(&T[i]);
        __m256d sigma_vec = _mm256_loadu_pd(&sigma[i]);
        
        __m256d result = blackscholes_put_avx(
            S_vec, K_vec, T_vec, r_vec, q_vec, sigma_vec);
        
        _mm256_storeu_pd(&prices[i], result);
    }
    
    for (; i < n; ++i) {
        prices[i] = blackScholesPutScalar(S[i], K[i], T[i], r, q, sigma[i]);
    }
#else
    for (int i = 0; i < n; ++i) {
        prices[i] = blackScholesPutScalar(S[i], K[i], T[i], r, q, sigma[i]);
    }
#endif
}

inline bool simdAvailable() {
#if defined(SIMD_AVX2_ENABLED) || defined(SIMD_AVX_ENABLED)
    return true;
#else
    return false;
#endif
}

inline const char* simdCapability() {
#if defined(SIMD_AVX2_ENABLED)
    return "AVX2";
#elif defined(SIMD_AVX_ENABLED)
    return "AVX";
#elif defined(SIMD_SSE42_ENABLED)
    return "SSE4.2";
#else
    return "Scalar";
#endif
}

} 

