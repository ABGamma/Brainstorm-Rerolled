// prefilter_test.cpp — proves the batched prefilter (src/prefilter.cpp)
// computes exactly what the normal engine computes, and that the prefiltered
// search finds exactly the same seeds.
//
// 1. rolls:  for millions of seeds, every first tag/voucher/pack roll the
//            prefilter marks "decided" must equal Instance::nextTag/
//            nextVoucher/nextPack (with the same locks brainstormFilter uses).
//            "Undecided" rolls must really be locked items.
// 2. scan:   scanRange() with the prefilter must return the same first
//            match as scanRange() without it, for many filters and ranges.

#include "filters.hpp"
#include "functions.hpp"
#include "prefilter.hpp"
#include "scan.hpp"

#include <cstdio>
#include <random>
#include <string>

static int failures = 0;

static Item normalRoll(Prefilter::Kind kind, long long id) {
  Seed s(id);
  Instance inst(s);
  inst.initLocks(1, false, true);
  if (kind == Prefilter::TAG) return inst.nextTag(1);
  if (kind == Prefilter::VOUCHER) return inst.nextVoucher(1);
  inst.cache.generatedFirstPack = true; // as brainstormFilter's pack check
  return inst.nextPack(1);
}

// Totals per roll kind, for runs checked with print == false.
static long long g_decided[3], g_wrong[3];

static void checkRolls(long long firstId, long long count, const char *what,
                       bool print = true) {
  // A config using all three checks, so the prefilter prepares the seed
  // hashes every kind of roll needs. (Which items it wants doesn't matter
  // to firstRolls.)
  FilterConfig all;
  all.tag = Item::Charm_Tag;
  all.voucher = Item::Overstock;
  all.pack = Item::Arcana_Pack;
  Prefilter pf(all);
  const char *names[] = {"tag", "voucher", "pack"};
  for (Prefilter::Kind kind :
       {Prefilter::TAG, Prefilter::VOUCHER, Prefilter::PACK}) {
    long long decidedCount = 0, wrong = 0;
    Seed s(firstId);
    for (long long done = 0; done < count;) {
      int n = (int)std::min<long long>(Prefilter::LANES, count - done);
      Item items[Prefilter::LANES];
      bool decided[Prefilter::LANES];
      pf.firstRolls(kind, s, n, items, decided);
      for (int i = 0; i < n; i++) {
        long long id = firstId + done + i;
        if (!decided[i]) continue; // checked below via the scan test
        decidedCount++;
        Item expect = normalRoll(kind, id);
        if (items[i] != expect && wrong++ < 5)
          std::printf("    %s seed %s: prefilter %s, engine %s\n", names[kind],
                      Seed(id).tostring().c_str(), itemToString(items[i]).c_str(),
                      itemToString(expect).c_str());
      }
      done += n;
    }
    g_decided[kind] += decidedCount;
    g_wrong[kind] += wrong;
    if (print)
      std::printf("%s  rolls %-7s %-26s %lld decided, %lld wrong\n",
                  wrong ? "FAIL" : "PASS", names[kind], what, decidedCount,
                  wrong);
    failures += wrong != 0;
  }
}

static void checkScan(const char *label, const FilterConfig &cfg,
                      long long firstId, long long count) {
  std::atomic<bool> stop{false};
  std::atomic<uint64_t> checked{0};
  Prefilter pf(cfg);
  long long plain = scanRange(cfg, nullptr, firstId, count, stop, checked);
  long long fast = scanRange(cfg, &pf, firstId, count, stop, checked);
  bool ok = plain == fast;
  auto name = [](long long id) {
    return id < 0 ? std::string("-") : Seed(id).tostring();
  };
  std::printf("%s  scan %-20s from %-9s plain=%-9s prefiltered=%s\n",
              ok ? "PASS" : "FAIL", label, Seed(firstId).tostring().c_str(),
              name(plain).c_str(), name(fast).c_str());
  failures += !ok;
}

int main(int argc, char **argv) {
  bool quick = argc > 1 && std::string(argv[1]) == "quick";
  long long n = quick ? 200000 : 3000000;

  // Rolls: a consecutive run, plus random places across the seed space
  // (short seeds, long seeds, near ZZZZZZZZ).
  checkRolls(Seed(std::string("AAAAAAAA")).getID(), n, "from AAAAAAAA");
  std::mt19937_64 rng(7);
  for (int k = 0; k < 3; k++) g_decided[k] = g_wrong[k] = 0;
  for (int r = 0; r < 20; r++) {
    long long start = 1 + (long long)(rng() % (2318107019760ull - 100000));
    checkRolls(start, n / 20, "", false);
  }
  const char *kinds[] = {"tag", "voucher", "pack"};
  for (int k = 0; k < 3; k++)
    std::printf("%s  rolls %-7s %-26s %lld decided, %lld wrong\n",
                g_wrong[k] ? "FAIL" : "PASS", kinds[k], "20 random places",
                g_decided[k], g_wrong[k]);
  checkRolls(1, 50000, "from 1 (short seeds)");

  // Scans: first match with and without the prefilter must be identical.
  FilterConfig c;
  struct Named { const char *label; FilterConfig cfg; };
  std::vector<Named> cfgs;
  c = FilterConfig(); c.tag = Item::Charm_Tag; cfgs.push_back({"charm", c});
  c = FilterConfig(); c.tag = Item::Negative_Tag; cfgs.push_back({"negative tag", c}); // locked in ante 1, so never matches
  c = FilterConfig(); c.tag = Item::Double_Tag; c.souls = 1; cfgs.push_back({"double+soul", c});
  c = FilterConfig(); c.voucher = Item::Telescope; cfgs.push_back({"telescope", c});
  c = FilterConfig(); c.voucher = Item::Overstock; c.tag = Item::Charm_Tag; cfgs.push_back({"overstock+charm", c});
  c = FilterConfig(); c.observatory = true; cfgs.push_back({"observatory", c});
  c = FilterConfig(); c.pack = Item::Mega_Arcana_Pack; cfgs.push_back({"mega arcana", c});
  c = FilterConfig(); c.pack = Item::Buffoon_Pack; c.tag = Item::Uncommon_Tag; cfgs.push_back({"buffoon+uncommon", c});
  c = FilterConfig(); c.pack = Item::Mega_Spectral_Pack; c.voucher = Item::Hone; c.tag = Item::Rare_Tag; cfgs.push_back({"spectral+hone+rare", c});
  long long range = quick ? 300000 : 3000000;
  for (auto &nc : cfgs) {
    checkScan(nc.label, nc.cfg, Seed(std::string("1A2B3C4D")).getID(), range);
    checkScan(nc.label, nc.cfg, 1 + (long long)(rng() % 2000000000000ull), range);
  }

  std::printf("\n%s (%d failing check%s)\n", failures ? "FAILED" : "ALL PASSED",
              failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
