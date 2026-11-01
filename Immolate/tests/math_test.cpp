// math_test.cpp — proves the fast paths in fract() and round13()
// (src/util.cpp) return exactly the same bits as the original code.
//
// ref_fract / ref_round13 below are verbatim copies of the original
// implementations (git history: util.cpp before the "fast paths" commit).
// The test feeds both versions hundreds of millions of inputs, including
// values deliberately placed on the exact rounding boundaries, and fails on
// the first bit difference.

#include "util.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>

static const double inv_prec = std::pow(10.0, 13);
static const double two_inv_prec = std::pow(2.0, 13);
static const double five_inv_prec = std::pow(5.0, 13);

// ---- original implementations (reference) ----------------------------------
double ref_fract(double x) {
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

double ref_round13(double x) {
  double normal_case = std::round(x * inv_prec) / inv_prec;
  if (normal_case ==
      (std::round(std::nextafter(x, -1) * inv_prec) / inv_prec)) {
    return normal_case;
  }
  double truncated = ref_fract(x * two_inv_prec) * five_inv_prec;
  if (ref_fract(truncated) >= 0.5) {
    return (std::floor(x * inv_prec) + 1) / inv_prec;
  }
  return std::floor(x * inv_prec) / inv_prec;
}
// ----------------------------------------------------------------------------

static int failures = 0;
static long long checks = 0;

static bool sameBits(double a, double b) {
  if (std::isnan(a) && std::isnan(b)) return true; // any NaN is a NaN
  return std::memcmp(&a, &b, sizeof a) == 0;
}

static void checkFract(double x) {
  checks++;
  if (!sameBits(fractFast(x), ref_fract(x)) && failures++ < 10)
    std::printf("  fractFast(%.17g) differs\n", x);
  double a = fract(x), b = ref_fract(x);
  if (!sameBits(a, b) && failures++ < 10)
    std::printf("FAIL fract(%.17g): new %.17g, original %.17g\n", x, a, b);
}

static void checkRound13(double x) {
  checks++;
  if (!sameBits(round13Fast(x), ref_round13(x)) && failures++ < 10)
    std::printf("  round13Fast(%.17g) differs\n", x);
  double a = round13(x), b = ref_round13(x);
  if (!sameBits(a, b) && failures++ < 10)
    std::printf("FAIL round13(%.17g): new %.17g, original %.17g\n", x, a, b);
}

// luaRandomTop16 must equal the top 16 bits of the full LuaRandom, and
// luaRandomDecide must give f(full random()) for monotone f.
static void checkLua(double seed, double threshold, int n) {
  checks++;
  LuaRandom full(seed);
  double x = full.random();
  bool ok = luaRandomTop16(seed) == (uint32_t)(x * 65536.0) &&
            luaRandomDecide(seed, [&](double v) { return v < threshold; }) ==
                (x < threshold) &&
            luaRandint(seed, 0, n - 1) == (int)(x * n);
  if (!ok && failures++ < 10)
    std::printf("  LuaRandom fast path(%.17g, %.17g, %d) differs\n", seed,
                threshold, n);
}

int main(int argc, char **argv) {
  long long n = argc > 1 ? std::atoll(argv[1]) : 20000000;
  std::mt19937_64 rng(12345);
  std::uniform_real_distribution<double> unit(0.0, 1.0);

  // 1. Special values for both functions.
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  double specials[] = {0.0, -0.0, 1.0, 0.5, 1e-300, 4.9e-324, 0.9999999999999999,
                       4503599627370495.5, 4503599627370496.0, 9007199254740993.0,
                       1e300, inf, -inf, nan, -0.5, -1.25, -1e-20};
  for (double x : specials) {
    checkFract(x);
    checkRound13(x);
  }

  // 2. Random values in [0, 1) -- what the RNG actually produces.
  for (long long i = 0; i < n; i++) {
    double x = unit(rng);
    checkFract(x);
    checkRound13(x);
  }

  // 3. Values right on the rounding boundaries: (k + 0.5) / 10^13, plus and
  //    minus a few ulps. These are where round13 must take the slow path.
  for (long long i = 0; i < n; i++) {
    double k = (double)(rng() % 10000000000000ull);
    double x = (k + 0.5) / inv_prec;
    for (int u = -4; u <= 4; u++) {
      double y = x;
      for (int s = 0; s < (u < 0 ? -u : u); s++)
        y = std::nextafter(y, u < 0 ? 0.0 : 1.0);
      checkRound13(y);
    }
  }

  // 4. fract on the range pseudohash feeds it (values up to ~1e6).
  std::uniform_real_distribution<double> wide(0.0, 2e6);
  for (long long i = 0; i < n; i++) checkFract(wide(rng));

  // 5. LuaRandom fast path. Seeds like the game's (in [0, 1)) and any other
  //    doubles; thresholds anywhere and right at a 1/65536 bucket edge (where
  //    the full computation is needed); every pool size the game uses.
  for (double x : specials) checkLua(x, 0.5, 7);
  std::uniform_int_distribution<int> sizes(1, 200);
  for (long long i = 0; i < n; i++) {
    double seed = i % 4 ? unit(rng) : wide(rng) - 1e6;
    double t = i % 2 ? unit(rng)
                     : (double)(rng() % 65536) / 65536 +
                           (double)((int)(rng() % 9) - 4) / 4503599627370496.0;
    checkLua(seed, t, sizes(rng));
  }

  std::printf("%s: %lld comparisons, %d mismatches\n",
              failures ? "FAILED" : "PASSED", checks, failures);
  return failures ? 1 : 0;
}
