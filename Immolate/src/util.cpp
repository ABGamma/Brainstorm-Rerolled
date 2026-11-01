#include "util.hpp"
#include <cstring>
#include <limits>

LuaRandom::LuaRandom(double seed) {
  double d = seed;
  uint64_t r = 0x11090601;
  for (int i = 0; i < 4; i++) {
    uint64_t m = 1ull << (r & 255);
    r >>= 8;
    d = d * 3.14159265358979323846 + 2.7182818284590452354;
    dbllong u;
    u.dbl = d;
    if (u.ulong < m)
      u.ulong += m;
    state[i] = u.ulong;
  }
  for (int i = 0; i < 10; i++) {
    _randint();
  }
}

LuaRandom::LuaRandom() { LuaRandom(0); }

namespace {
// One LuaRandom update step of state component c (see _randint below).
uint64_t luaStep(int c, uint64_t z) {
  switch (c) {
  case 0: return (((z << 31) ^ z) >> 45) ^ ((z & (MAX_UINT64 << 1)) << 18);
  case 1: return (((z << 19) ^ z) >> 30) ^ ((z & (MAX_UINT64 << 6)) << 28);
  case 2: return (((z << 24) ^ z) >> 48) ^ ((z & (MAX_UINT64 << 9)) << 7);
  default: return (((z << 21) ^ z) >> 39) ^ ((z & (MAX_UINT64 << 17)) << 8);
  }
}

// TOP16[c][p][v]: bits 51..36 (the top 16 bits of random()'s mantissa) that
// byte p of state component c contributes when it holds the value v, after
// 11 steps. Because the steps are linear, the contributions of all 32 bytes
// simply XOR together. 16 KB, built once when the library loads.
struct Top16Table {
  uint16_t t[4][8][256];
  Top16Table() {
    for (int c = 0; c < 4; c++)
      for (int p = 0; p < 8; p++)
        for (int v = 0; v < 256; v++) {
          uint64_t z = (uint64_t)v << (8 * p);
          for (int i = 0; i < 11; i++) z = luaStep(c, z);
          t[c][p][v] = (uint16_t)(z >> 36);
        }
  }
};
const Top16Table TOP16;
} // namespace

uint32_t luaRandomTop16(double seed) {
  // Exactly the start of LuaRandom::LuaRandom: four states from `seed`.
  double d = seed;
  uint64_t r = 0x11090601;
  uint32_t out = 0;
  for (int i = 0; i < 4; i++) {
    uint64_t m = 1ull << (r & 255);
    r >>= 8;
    d = d * 3.14159265358979323846 + 2.7182818284590452354;
    uint64_t u;
    std::memcpy(&u, &d, sizeof u);
    if (u < m)
      u += m;
    // Written out (not a loop) so the compiler emits straight-line code.
    const uint16_t(*t)[256] = TOP16.t[i];
    out ^= t[0][u & 255] ^ t[1][(u >> 8) & 255] ^ t[2][(u >> 16) & 255] ^
           t[3][(u >> 24) & 255] ^ t[4][(u >> 32) & 255] ^
           t[5][(u >> 40) & 255] ^ t[6][(u >> 48) & 255] ^ t[7][u >> 56];
  }
  return out;
}

uint64_t LuaRandom::_randint() {
  uint64_t z = 0;
  uint64_t r = 0;
  z = state[0];
  z = (((z << 31ull) ^ z) >> 45ull) ^ ((z & (MAX_UINT64 << 1ull)) << 18ull);
  r ^= z;
  state[0] = z;
  z = state[1];
  z = (((z << 19ull) ^ z) >> 30ull) ^ ((z & (MAX_UINT64 << 6ull)) << 28ull);
  r ^= z;
  state[1] = z;
  z = state[2];
  z = (((z << 24ull) ^ z) >> 48ull) ^ ((z & (MAX_UINT64 << 9ull)) << 7ull);
  r ^= z;
  state[2] = z;
  z = state[3];
  z = (((z << 21ull) ^ z) >> 39ull) ^ ((z & (MAX_UINT64 << 17ull)) << 8ull);
  r ^= z;
  state[3] = z;
  return r;
}

uint64_t LuaRandom::randdblmem() {
  return (_randint() & 4503599627370495ull) | 4607182418800017408ull;
}

double LuaRandom::random() {
  dbllong u;
  u.ulong = randdblmem();
  return u.dbl - 1.0;
}

int LuaRandom::randint(int min, int max) {
  return (int)(random() * (max - min + 1)) + min;
}

int portable_clzll(uint64_t x) {
  if (x == 0)
    return 64; // Undefined for 0, by convention we return 64

#if defined(__GNUC__) || defined(__clang__)
  return __builtin_clzll(x);
#elif defined(_MSC_VER)
  unsigned long index;
  if (_BitScanReverse64(&index, x)) {
    return 63 - index;
  }
  return 64;
#else
  // Fallback for other compilers (manual bit manipulation)
  int n = 0;
  if (x <= 0x00000000FFFFFFFF) {
    n += 32;
    x <<= 32;
  }
  if (x <= 0x0000FFFFFFFFFFFF) {
    n += 16;
    x <<= 16;
  }
  if (x <= 0x00FFFFFFFFFFFFFF) {
    n += 8;
    x <<= 8;
  }
  if (x <= 0x0FFFFFFFFFFFFFFF) {
    n += 4;
    x <<= 4;
  }
  if (x <= 0x3FFFFFFFFFFFFFFF) {
    n += 2;
    x <<= 2;
  }
  if (x <= 0x7FFFFFFFFFFFFFFF) {
    n += 1;
  }
  return n;
#endif
}

double fract(double x) {
  // Fast path (performance): for 0 <= x < 2^52, x - trunc(x) is exactly the
  // fractional part, and truncating to an integer is one cheap instruction.
  // Negative, huge, infinite and NaN inputs use the original code below.
  // tests/math_test.cpp checks this against the original bit for bit.
  if (x >= 0.0 && x < 4503599627370496.0) {
    return x - (double)(int64_t)x;
  }

  uint64_t x_int;

  std::memcpy(&x_int, &x, sizeof(x_int));

  uint64_t expo = (x_int & DBL_EXPO) >> DBL_MANT_SZ;
  if (expo < DBL_EXPO_BIAS) {
    return x;
  }
  if (expo == ((1 << DBL_EXPO_SZ) - 1)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  uint64_t expo_biased = expo - DBL_EXPO_BIAS;
  if (expo_biased >= DBL_MANT_SZ) {
    return 0;
  }
  uint64_t mant = x_int & DBL_MANT;
  uint64_t frac_mant = mant & ((1ull << (DBL_MANT_SZ - expo_biased)) - 1);
  if (frac_mant == 0) {
    return 0;
  }
  uint64_t frac_lzcnt = portable_clzll(frac_mant) - (64 - DBL_MANT_SZ);
  uint64_t res_expo = (expo - frac_lzcnt - 1) << DBL_MANT_SZ;
  uint64_t res_mant = (frac_mant << (frac_lzcnt + 1)) & DBL_MANT;
  uint64_t res = res_expo | res_mant;

  double result;
  std::memcpy(&result, &res, sizeof(result));
  return result;
}

double pseudohash(const std::string &s) {
  double num = 1;
  for (size_t i = s.length(); i > 0; i--) {
    num = fractFast(1.1239285023 / num * s[i - 1] * 3.141592653589793116 +
                3.141592653589793116 * i);
  }
  return num;
}

double pseudohash_from(const std::string &s, double num) {
  for (size_t i = s.length(); i > 0; i--) {
    num = fractFast(1.1239285023 / num * s[i - 1] * 3.141592653589793116 +
                3.141592653589793116 * i);
  }
  return num;
}

double pseudostep(char s, int pos, double num) {
  return fractFast(1.1239285023 / num * s * 3.141592653589793116 +
               3.141592653589793116 * pos);
}

std::string anteToString(int a) {
  if (a < 10)
    return {(char)(0x30 + a)};
  else
    return {(char)(0x30 + a / 10), (char)(0x30 + a % 10)};
}

const double inv_prec = std::pow(10.0, 13);
const double two_inv_prec = std::pow(2.0, 13);
const double five_inv_prec = std::pow(5.0, 13);

double round13(double x) {
  // Fast path (performance). Rounding x*10^13 is only ambiguous when its
  // fractional part is very close to .5; the original code below detects
  // that by also rounding the next-smaller double, which can move x*10^13
  // by at most ~0.002 for x < 1 (one ulp of x is <= 1.1e-16, times 10^13,
  // plus the rounding of x * 10^13 itself). When the fractional part is
  // clearly away from .5 (by 0.01, five times that), the rounding is
  // unambiguous, so we return the same value the original would, with the
  // same division.
  // The hot path (Instance::advance_node) uses the branch-free inline copy
  // of this, round13Fast in util.hpp; this one handles everything else.
  // tests/math_test.cpp checks this against the original bit for bit.
  // (x > 0, not >= 0: for -0.0 the original returns -0.0, and the integer
  // conversion below would lose the sign. math_test caught this.)
  if (x > 0.0 && x < 1.0) {
    double y = x * inv_prec;
    double whole = (double)(int64_t)y; // y < 10^13, so this is floor(y)
    double frac = y - whole;
    if (frac < 0.49 || frac > 0.51)
      return (whole + (frac > 0.5 ? 1.0 : 0.0)) / inv_prec;
  }

  double normal_case = std::round(x * inv_prec) / inv_prec;
  if (normal_case ==
      (std::round(std::nextafter(x, -1) * inv_prec) / inv_prec)) {
    return normal_case;
  }
  double truncated = fract(x * two_inv_prec) * five_inv_prec;
  if (fract(truncated) >= 0.5) {
    return (std::floor(x * inv_prec) + 1) / inv_prec;
  }
  return std::floor(x * inv_prec) / inv_prec;
}