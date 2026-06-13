

#pragma once

#if defined(__cplusplus) && __cplusplus >= 202002L
    
    #define VOL_LIKELY(x)   (x) [[likely]]
    #define VOL_UNLIKELY(x) (x) [[unlikely]]
#elif defined(_MSC_VER)

    #define VOL_LIKELY(x)   (x)
    #define VOL_UNLIKELY(x) (x)
#elif defined(__GNUC__) || defined(__clang__)
    
    #define VOL_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define VOL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    
    #define VOL_LIKELY(x)   (x)
    #define VOL_UNLIKELY(x) (x)
#endif

#if defined(_MSC_VER)
    #include <intrin.h>
    
    #define VOL_PREFETCH_READ(addr)  _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0)
    #define VOL_PREFETCH_WRITE(addr) _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0)
    #define VOL_PREFETCH_NTA(addr)   _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_NTA)
#elif defined(__GNUC__) || defined(__clang__)

    #define VOL_PREFETCH_READ(addr)  __builtin_prefetch((addr), 0, 3)
    #define VOL_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
    #define VOL_PREFETCH_NTA(addr)   __builtin_prefetch((addr), 0, 0)
#else
    
    #define VOL_PREFETCH_READ(addr)  ((void)(addr))
    #define VOL_PREFETCH_WRITE(addr) ((void)(addr))
    #define VOL_PREFETCH_NTA(addr)   ((void)(addr))
#endif

constexpr size_t VOL_CACHE_LINE_SIZE = 64;

#if defined(_MSC_VER)
    #define VOL_CACHELINE_ALIGN __declspec(align(64))
#elif defined(__GNUC__) || defined(__clang__)
    #define VOL_CACHELINE_ALIGN __attribute__((aligned(64)))
#else
    #define VOL_CACHELINE_ALIGN
#endif

#define VOL_CACHE_ALIGNED alignas(VOL_CACHE_LINE_SIZE)

#if defined(_MSC_VER)
    
    #define VOL_LOOP_VECTORIZE __pragma(loop(hint_parallel(4)))
    #define VOL_LOOP_UNROLL(n) __pragma(loop(unroll_count(n)))
#elif defined(__clang__)
    
    #define VOL_LOOP_VECTORIZE _Pragma("clang loop vectorize(enable)")
    #define VOL_LOOP_UNROLL(n) _Pragma("clang loop unroll_count(" #n ")")
#elif defined(__GNUC__)
    
    #define VOL_LOOP_VECTORIZE _Pragma("GCC ivdep")
    #define VOL_LOOP_UNROLL(n) _Pragma("GCC unroll " #n)
#else
    #define VOL_LOOP_VECTORIZE
    #define VOL_LOOP_UNROLL(n)
#endif

#if defined(_MSC_VER)
    #define VOL_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
    #define VOL_RESTRICT __restrict__
#else
    #define VOL_RESTRICT
#endif

#if defined(_MSC_VER)
    #define VOL_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define VOL_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define VOL_FORCE_INLINE inline
#endif

#if defined(_MSC_VER)
    #define VOL_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define VOL_NOINLINE __attribute__((noinline))
#else
    #define VOL_NOINLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define VOL_HOT  __attribute__((hot))
    #define VOL_COLD __attribute__((cold))
#else
    #define VOL_HOT
    #define VOL_COLD
#endif

namespace vol_opt {

template<typename T>
VOL_FORCE_INLINE T branchfree_min(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T>
VOL_FORCE_INLINE T branchfree_max(T a, T b) {
    return (a > b) ? a : b;
}

template<typename T>
VOL_FORCE_INLINE T branchfree_clamp(T val, T lo, T hi) {
    return branchfree_min(branchfree_max(val, lo), hi);
}

template<typename T>
VOL_FORCE_INLINE int branchfree_sign(T val) {
    return (T(0) < val) - (val < T(0));
}

} 

