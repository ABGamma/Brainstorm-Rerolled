#ifndef UTIL_HPP
#define UTIL_HPP

#include <cmath>
#include <cstdint>
#include <string>
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#endif

const uint64_t MAX_UINT64 = 18446744073709551615ull;

typedef union DoubleLong {
  double dbl;
  uint64_t ulong;
} dbllong;

struct LuaRandom {
  uint64_t state[4];
  LuaRandom(double seed);
  LuaRandom();
  uint64_t _randint();
  uint64_t randdblmem();
  double random();
  int randint(int min, int max);
};

double fract(double x);
double round13(double x);

// 1.0 if a > b, else 0.0, computed WITHOUT a branch. Used where a > b is a
// coin flip: a branch there is mispredicted half the time, and each
// misprediction throws away the work the CPU had started in parallel
// (measured: that alone made the stream step ~2x slower). Compilers turn a
// plain `a > b ? 1.0 : 0.0` back into a branch on x86, so SSE2 (present on
// every x86-64 CPU) is used directly there; ARM compilers already emit a
// branchless select.
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
inline double oneIfGreater(double a, double b) {
  __m128d mask = _mm_cmpgt_sd(_mm_set_sd(a), _mm_set_sd(b)); // all 1s or 0s
  return _mm_cvtsd_f64(_mm_and_pd(mask, _mm_set_sd(1.0)));
}
#else
inline double oneIfGreater(double a, double b) { return a > b ? 1.0 : 0.0; }
#endif

// floor(x) for 0 <= x < 2^52, using only floating-point adds (exact):
// x + 2^52 lands in [2^52, 2^53), where doubles are exactly the integers, so
// it is x rounded to the nearest integer; subtracting 2^52 again is exact.
// If that rounded up, take 1 off. Same value as (double)(int64_t)x, without
// the slow trip through an integer register.
inline double floorSmall(double x) {
  double r = (x + 4503599627370496.0) - 4503599627370496.0;
  return r - oneIfGreater(r, x);
}

// Inline copies of the fast paths of fract() and round13() (util.cpp), for
// the hottest loop (Instance::advance_node): same values, branch-free, and
// anything else goes to the real function. tests/math_test.cpp checks them.
inline double fractFast(double x) {
  if (x >= 0.0 && x < 4503599627370496.0) return x - floorSmall(x);
  return fract(x);
}
inline double round13Fast(double x) {
  if (x > 0.0 && x < 1.0) {
    double y = x * 10000000000000.0; // inv_prec in util.cpp
    double whole = floorSmall(y);
    double frac = y - whole;
    // Away from .5 by more than 0.01 (see round13 in util.cpp for why that
    // is safe): true 98% of the time, so this branch is well predicted.
    if (std::fabs(frac - 0.5) > 0.01)
      return (whole + oneIfGreater(frac, 0.5)) / 10000000000000.0;
  }
  return round13(x);
}

// ---------------------------------------------------------------------------
// Fast path for LuaRandom: the first random() value, to 16 bits.
//
// LuaRandom(seed) turns `seed` into four 64-bit states, then runs 11 steps
// (10 warm-up + the random() call) of an update that only shifts, ANDs with
// constants and XORs. Such an update is *linear over bits*: every output bit
// is the XOR of a fixed set of starting-state bits. So the top 16 bits of
// random() are the XOR of one precomputed table entry per byte of the four
// states: 32 lookups instead of 44 state updates (about 2.4x faster).
//
// luaRandomTop16(seed) == (int)(LuaRandom(seed).random() * 65536), always.
// Checked by tests/math_test.cpp.
uint32_t luaRandomTop16(double seed);

// f(LuaRandom(seed).random()) for a function f that is MONOTONE in its
// argument (never goes up then down, e.g. "x < 0.7", "(int)(x * n)").
//
// Using the 16 bits, random() is known to lie in [lo, hi], an interval of
// width 1/65536. If f(lo) == f(hi), then f has that value on the whole
// interval (that is what monotone means), so it is the answer. Otherwise
// (about once per 65536 draws per boundary) the full LuaRandom is run.
// f must return something comparable with ==, ideally an int index.
template <class F>
inline auto luaRandomDecide(double seed, F f) -> decltype(f(0.0)) {
  // random() is m / 2^52 for an integer m; its top 16 bits are t, so
  // t/2^16 <= random() <= t/2^16 + 2^-16 - 2^-52. Both ends are exact.
  double lo = (double)luaRandomTop16(seed) * (1.0 / 65536);
  double hi = lo + (1.0 / 65536 - 1.0 / 4503599627370496.0);
  auto a = f(lo);
  if (a == f(hi)) return a;
  LuaRandom rng(seed);
  return f(rng.random());
}

// LuaRandom(seed).randint(min, max), via luaRandomDecide.
inline int luaRandint(double seed, int min, int max) {
  return luaRandomDecide(
      seed, [&](double x) { return (int)(x * (max - min + 1)) + min; });
}

#define DBL_EXPO 0x7FF0000000000000
#define DBL_MANT 0x000FFFFFFFFFFFFF

#define DBL_EXPO_SZ 11
#define DBL_MANT_SZ 52

#define DBL_EXPO_BIAS 1023

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
#endif

int portable_clzll(uint64_t x);
double fract(double x);
double pseudohash(const std::string &s);
double pseudohash_from(const std::string &s, double num);
double pseudostep(char s, int pos, double num);
std::string anteToString(int a);
double round13(double x);

#endif // UTIL_HPP