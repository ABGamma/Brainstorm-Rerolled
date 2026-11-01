// bench.cpp — measures how fast the CPU engine checks seeds.
//
//   bench            single-thread speed for several representative filters
//   bench threads    also the full multi-threaded search speed
//
// Every filter here is made impossible to satisfy (Observatory as the ante 1
// voucher can never happen), so each seed runs the whole filter and nothing
// stops early. Numbers are seeds per second; higher is better.
//
// Run it before and after a performance change, on an otherwise idle
// machine. Results vary a few percent between runs.

#include "brainstorm_api.h"
#include "filters.hpp"
#include "functions.hpp"
#include "prefilter.hpp"
#include "scan.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

struct Case {
  const char *label;
  FilterConfig cfg;
};

static double secondsSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
      .count();
}

int main(int argc, char **argv) {
  bool threads = argc > 1 && std::string(argv[1]) == "threads";

  // Each case puts the expensive work BEFORE the impossible voucher check
  // where possible, so the work being measured really happens.
  FilterConfig tag, soul, voucher, pack, obs, bean;
  tag.tag = Item::Charm_Tag;           // tag roll (fails ~93% here)
  soul.souls = 1;                      // Mega Arcana pack roll
  voucher.voucher = Item::Observatory; // impossible: voucher roll + resamples
  pack.pack = Item::Mega_Arcana_Pack;  // shop pack roll
  obs.observatory = true;              // voucher, then ante 2 voucher
  bean.bean = true;                    // up to 150 ante-8 shop items
  Case cases[] = {
      {"tag (Charm Tag)", tag},
      {"souls (1 Soul in Mega Arcana)", soul},
      {"voucher (impossible)", voucher},
      {"pack (Mega Arcana)", pack},
      {"observatory", obs},
      {"bean (150 shop items)", bean},
  };

  std::printf("single thread:\n");
  for (Case &c : cases) {
    Seed s(std::string("BENCH111"));
    Instance inst(s);
    long long n = 0, hits = 0;
    auto t0 = std::chrono::steady_clock::now();
    // Run for ~1.5 s per case, checking the clock every 4096 seeds.
    while (true) {
      for (int i = 0; i < 4096; i++, n++) {
        hits += brainstormFilter(inst, c.cfg) >= 1;
        inst.next();
      }
      if (secondsSince(t0) > 1.5) break;
    }
    double secs = secondsSince(t0);
    double plainRate = n / secs / 1e6;

    // Same filter through scanRange() with the prefilter (the path real
    // searches take). Restarts after each match until ~1.5 s have passed.
    Prefilter pf(c.cfg);
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> checked{0};
    long long id = Seed(std::string("BENCH111")).getID();
    auto t1 = std::chrono::steady_clock::now();
    while (secondsSince(t1) < 1.5) {
      long long found = scanRange(c.cfg, &pf, id, 1 << 16, stop, checked);
      id = found >= 0 ? found + 1 : id + (1 << 16);
    }
    long long scanned = id - Seed(std::string("BENCH111")).getID();
    double fastRate = scanned / secondsSince(t1) / 1e6;
    std::printf("  %-32s %8.3f M seeds/s   prefiltered %8.3f M/s %s\n",
                c.label, plainRate, fastRate,
                pf.active() ? "" : "(n/a: no first roll to batch)");
  }

  if (threads) {
    std::printf("all threads (real search through the API):\n");
    bs_params p = {};
    p.api_version = BS_API_VERSION;
    p.start_seed = "BENCH111";
    p.tag = "tag_charm";
    p.voucher = "v_observatory"; // impossible: runs until cancelled
    p.pack = "";
    p.custom_filter = "No Filter";
    bs_job *job = bs_search_start(&p);
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // warm up
    uint64_t c0 = 0, c1 = 0;
    bs_search_poll(job, nullptr, &c0);
    auto t0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    bs_search_poll(job, nullptr, &c1);
    double secs = secondsSince(t0);
    bs_search_free(job);
    std::printf("  %-32s %8.3f M seeds/s   (%s)\n", "Charm Tag + voucher",
                (c1 - c0) / secs / 1e6, bs_build_info());
  }
  return 0;
}
