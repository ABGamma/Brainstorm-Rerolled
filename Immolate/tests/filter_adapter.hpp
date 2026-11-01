// Adapter between the golden harness and the filter implementation.
//
// The harness only talks to the filter through applyConfig(), runFilter() and
// searchOneThread(), so filter internals can be rewritten without touching
// golden.cpp. The very first golden_expected.txt was produced by an adapter
// for the original global-variable code (see git history of this file).
#pragma once
#include "../src/brainstorm_api.h"
#include "../src/filters.hpp"
#include <string>

struct TestConfig {
  Item tag = Item::RETRY, voucher = Item::RETRY, pack = Item::RETRY;
  double souls = 0;
  bool observatory = false, perkeo = false, copymoney = false, retcon = false,
       bean = false, burglar = false;
  std::string custom = "No Filter";
};

inline TestConfig g_test;
inline FilterConfig g_cfg;

inline void applyConfig(const TestConfig &c) {
  g_test = c;
  g_cfg = FilterConfig();
  g_cfg.tag = c.tag;
  g_cfg.voucher = c.voucher;
  g_cfg.pack = c.pack;
  g_cfg.souls = (int)c.souls;
  g_cfg.observatory = c.observatory;
  g_cfg.perkeo = c.perkeo;
  g_cfg.copymoney = c.copymoney;
  g_cfg.retcon = c.retcon;
  g_cfg.bean = c.bean;
  g_cfg.burglar = c.burglar;
  g_cfg.custom = stringToFilter(c.custom);
}

inline long runFilter(Instance &inst) { return brainstormFilter(inst, g_cfg); }

// Runs a real search through the public C API with one thread, so the
// result is deterministic. Items are passed as English names, which also
// exercises the legacy name lookup.
inline std::string searchOneThread(const std::string &start) {
  auto nameOf = [](Item i) {
    return i == Item::RETRY ? std::string() : itemToString(i);
  };
  std::string tag = nameOf(g_test.tag), voucher = nameOf(g_test.voucher),
              pack = nameOf(g_test.pack);
  bs_params p = {};
  p.api_version = BS_API_VERSION;
  p.start_seed = start.c_str();
  p.threads = 1;
  p.tag = tag.c_str();
  p.voucher = voucher.c_str();
  p.pack = pack.c_str();
  p.souls = (int32_t)g_test.souls;
  p.observatory = g_test.observatory;
  p.perkeo = g_test.perkeo;
  p.copy_money = g_test.copymoney;
  p.retcon = g_test.retcon;
  p.bean = g_test.bean;
  p.burglar = g_test.burglar;
  p.custom_filter = g_test.custom.c_str();

  bs_job *job = bs_search_start(&p);
  if (!job) return std::string("ERROR: ") + bs_last_error();
  char seed[9] = {0};
  while (bs_search_poll(job, seed, nullptr) == BS_RUNNING) {
  }
  bs_search_free(job);
  return seed;
}
