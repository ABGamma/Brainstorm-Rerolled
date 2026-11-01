// api_test.cpp — checks the public C API (brainstorm_api.h) behaves as
// documented. Complements the golden test, which checks filter *results*.
//
// Each CHECK prints PASS/FAIL; the program exits non-zero on any failure.

#include "../src/brainstorm_api.h"
#include "../src/immolate.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

static int failures = 0;
#define CHECK(cond, what)                                                      \
  do {                                                                         \
    bool ok_ = (cond);                                                         \
    std::printf("%s  %s\n", ok_ ? "PASS" : "FAIL", what);                      \
    if (!ok_) failures++;                                                      \
  } while (0)

static bs_params blank() {
  bs_params p = {};
  p.api_version = BS_API_VERSION;
  p.tag = p.voucher = p.pack = "";
  p.custom_filter = "No Filter";
  return p;
}

// Waits (up to `seconds`) for a job to leave the RUNNING state.
static int waitDone(bs_job *job, char *seed, uint64_t *checked,
                    double seconds = 60) {
  auto t0 = std::chrono::steady_clock::now();
  int st;
  while ((st = bs_search_poll(job, seed, checked)) == BS_RUNNING) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::chrono::duration<double> el = std::chrono::steady_clock::now() - t0;
    if (el.count() > seconds) break;
  }
  return st;
}

int main() {
  CHECK(bs_api_version() == BS_API_VERSION, "bs_api_version matches header");
  std::printf("      build info: %s\n", bs_build_info());

  // --- Known seeds ---------------------------------------------------------
  {
    // Found with the CPU searcher (all cores).
    bs_params p = blank();
    p.tag = "tag_charm";
    p.souls = 2;
    CHECK(bs_check_seed(&p, "OLA2CAAA") == 1, "charm+2 souls: OLA2CAAA");
    CHECK(bs_check_seed(&p, "6W8F8AZ9") == 1, "charm+2 souls: 6W8F8AZ9");
    CHECK(bs_check_seed(&p, "1A2B3C4D") == 0, "charm+2 souls rejects 1A2B3C4D");
  }
  {
    // From golden_expected.txt: SNGAAAAA passes charm+soul+perkeo.
    bs_params p = blank();
    p.tag = "tag_charm";
    p.souls = 1;
    p.perkeo = 1;
    CHECK(bs_check_seed(&p, "SNGAAAAA") == 1, "charm+soul+perkeo: SNGAAAAA");
  }

  // --- Game keys and English names select the same item -------------------
  {
    const char *pairs[][2] = {{"tag_charm", "Charm Tag"},
                              {"tag_d_six", "D6 Tag"},
                              {"v_telescope", "Telescope"},
                              {"v_overstock_norm", "Overstock"},
                              {"p_arcana_mega", "Mega Arcana Pack"},
                              {"p_buffoon_normal", "Buffoon Pack"}};
    for (auto &kv : pairs) {
      int same = 0;
      for (int i = 0; i < 300; i++) {
        char seed[9];
        std::snprintf(seed, sizeof seed, "%dQ%dX", i % 9 + 1, i);
        for (char *c = seed; *c; c++)
          if (*c == '0') *c = 'Z'; // '0' is not a valid seed character
        bs_params a = blank(), b = blank();
        const char *k = kv[0];
        if (k[0] == 't') a.tag = kv[0], b.tag = kv[1];
        if (k[0] == 'v') a.voucher = kv[0], b.voucher = kv[1];
        if (k[0] == 'p') a.pack = kv[0], b.pack = kv[1];
        same += bs_check_seed(&a, seed) == bs_check_seed(&b, seed);
      }
      std::string what = std::string("key ") + kv[0] + " == name '" + kv[1] + "'";
      CHECK(same == 300, what.c_str());
    }
  }

  // --- Removed custom filters are ignored, not errors ----------------------
  {
    // Old saved settings may still name a removed custom filter; it must
    // behave like "No Filter" rather than fail.
    bs_params a = blank(), b = blank();
    a.tag = b.tag = "tag_charm";
    a.custom_filter = "Negative Perkeo";
    int same = 0;
    for (const char *seed : {"OLA2CAAA", "1A2B3C4D", "3BAAAAAA", "6BAAAAAA"})
      same += bs_check_seed(&a, seed) == bs_check_seed(&b, seed);
    CHECK(same == 4, "removed custom filter name acts as No Filter");
  }

  // --- Bad input is rejected with a message, never a crash ----------------
  {
    bs_params p = blank();
    p.tag = "tag_nonexistent";
    CHECK(bs_search_start(&p) == nullptr, "unknown tag rejected");
    CHECK(std::strstr(bs_last_error(), "tag_nonexistent") != nullptr,
          "error message names the bad tag");

    p = blank();
    p.start_seed = "abc0";
    CHECK(bs_search_start(&p) == nullptr, "invalid start seed rejected");

    p = blank();
    p.api_version = BS_API_VERSION + 1;
    CHECK(bs_search_start(&p) == nullptr, "API version mismatch rejected");

    CHECK(bs_search_start(nullptr) == nullptr, "NULL params rejected");
    CHECK(bs_search_poll(nullptr, nullptr, nullptr) == BS_ERROR,
          "poll(NULL) returns BS_ERROR");
    bs_search_free(nullptr); // must not crash
    CHECK(true, "free(NULL) is a no-op");
  }

  // --- A search finds a seed that actually passes -------------------------
  {
    bs_params p = blank();
    p.tag = "tag_charm";
    p.souls = 1;
    p.start_seed = "QWERTY12";
    bs_job *job = bs_search_start(&p);
    char seed[9] = {0};
    uint64_t checked = 0;
    int st = waitDone(job, seed, &checked);
    bs_search_free(job);
    CHECK(st == BS_FOUND, "multi-threaded search finds charm+soul");
    CHECK(bs_check_seed(&p, seed) == 1, "found seed passes bs_check_seed");
    std::printf("      found %s after ~%llu seeds\n", seed,
                (unsigned long long)checked);
  }

  // --- Cancelling stops promptly -------------------------------------------
  {
    // Impossible filter: Observatory needs Telescope first, so it can never
    // be the ante 1 voucher. The search would run for hours if not cancelled.
    bs_params p = blank();
    p.tag = "tag_charm";
    p.voucher = "v_observatory";
    bs_job *job = bs_search_start(&p);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    uint64_t before = 0;
    CHECK(bs_search_poll(job, nullptr, &before) == BS_RUNNING,
          "impossible search is still running after 200 ms");
    CHECK(before > 0, "progress counter advances");
    auto t0 = std::chrono::steady_clock::now();
    bs_search_cancel(job);
    int st = waitDone(job, nullptr, nullptr, 5);
    bs_search_free(job);
    std::chrono::duration<double> el = std::chrono::steady_clock::now() - t0;
    CHECK(st == BS_CANCELLED, "poll reports CANCELLED");
    CHECK(el.count() < 1.0, "cancel + free completes in under a second");
  }

  // --- Wrap-around past ZZZZZZZZ -------------------------------------------
  {
    // Start at the very last seed with a rare filter (~1 in 1500), so the
    // search must wrap around to "1", "11", ... and still return a valid
    // passing seed. The original code ran off the end into garbage here.
    bs_params p = blank();
    p.tag = "tag_charm";
    p.souls = 1;
    p.threads = 1;
    p.start_seed = "ZZZZZZZZ";
    CHECK(bs_check_seed(&p, "ZZZZZZZZ") == 0, "ZZZZZZZZ itself does not match");
    bs_job *job = bs_search_start(&p);
    char seed[9] = {0};
    int st = waitDone(job, seed, nullptr);
    bs_search_free(job);
    CHECK(st == BS_FOUND && bs_check_seed(&p, seed) == 1,
          "search wrapping past ZZZZZZZZ returns a valid passing seed");
    std::printf("      found %s\n", seed);
  }

  // --- Legacy blocking API still works ------------------------------------
  {
    const char *r = brainstorm("AAAAAAAA", "", "", "Charm Tag", 1, false, false,
                               false, false, false, false, "No Filter");
    bs_params p = blank();
    p.tag = "Charm Tag";
    p.souls = 1;
    CHECK(std::strlen(r) > 0 && bs_check_seed(&p, r) == 1,
          "legacy brainstorm() returns a passing seed");
    free_result(r);
  }

  std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
              failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
