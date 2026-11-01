// fastscan_test.cpp — proves src/fastscan.cpp gives exactly the answers of
// the full generators it replaces.
//
// 1. items:   ShopJokerScan(inst, ante, rarities, editions).next() vs
//             inst.nextShopItem(ante), item by item, for every ante, every
//             combination of rarities, with and without editions, locks,
//             vouchers and decks that change the shop. Also findShopJoker()
//             vs a plain loop, and the exact() guard.
// 2. souls:   arcanaPackHasSoul() vs nextArcanaPack() containing The Soul,
//             several packs in a row, plus the cases that take the slow path.
// 3. filters: brainstormFilter() vs a copy of the original Bean / Burglar /
//             Souls code (written with the full generators), for many seeds
//             and filter combinations.

#include "fastscan.hpp"
#include "filters.hpp"
#include "functions.hpp"

#include <cstdio>
#include <string>

static int failures = 0;

static void report(bool ok, const std::string &what, long long checked,
                   long long wrong) {
  std::printf("%s  %-46s %10lld checked, %lld wrong\n", ok ? "PASS" : "FAIL",
              what.c_str(), checked, wrong);
  failures += !ok;
}

// The ways a test seed's Instance can be set up before scanning.
enum Setup { PLAIN, LOCKED, MERCHANTS, TYCOONS_GHOST, MAGIC_TRICK, OLD_VERSION,
             HONE, GLOW_UP };
static const char *setupName[] = {"no locks",    "run-start locks",
                                  "merchants",   "tycoons + Ghost Deck",
                                  "Magic Trick", "version 1.0.0",
                                  "Hone",        "Glow Up"};

static void setup(Instance &inst, Setup s) {
  if (s != PLAIN) inst.initLocks(1, false, true);
  if (s == MERCHANTS) {
    inst.activateVoucher(Item::Tarot_Merchant);
    inst.activateVoucher(Item::Planet_Merchant);
  }
  if (s == TYCOONS_GHOST) {
    inst.activateVoucher(Item::Tarot_Tycoon);
    inst.activateVoucher(Item::Planet_Tycoon);
    inst.setDeck(Item::Ghost_Deck);
  }
  if (s == MAGIC_TRICK) inst.activateVoucher(Item::Magic_Trick);
  if (s == OLD_VERSION) inst.params.version = 10014;
  if (s == HONE) inst.activateVoucher(Item::Hone);
  if (s == GLOW_UP) inst.activateVoucher(Item::Glow_Up);
}

static void testItems(int seeds, int items) {
  for (int s = PLAIN; s <= GLOW_UP; s++) {
    long long checked = 0, wrong = 0, jokersSeen = 0, editionsSeen = 0;
    Seed seed(std::string("FASTSCN1"));
    for (int k = 0; k < seeds; k++, seed.next()) {
      for (int ante = 1; ante <= 8; ante++) {
        // Every rarity combination, with and without editions, spread over
        // the seeds (all 14 per seed would make the test slow).
        unsigned mask = 1 + (unsigned)((k + ante) % 7);
        bool editions = ((k / 7 + ante) & 1) != 0;
        Instance full(seed), fast(seed);
        setup(full, (Setup)s);
        setup(fast, (Setup)s);
        ShopJokerScan scan(fast, ante, mask, editions);
        for (int i = 0; i < items; i++) {
          ShopItem it = full.nextShopItem(ante);
          bool wanted = it.type == Item::T_Joker &&
                        (mask & shopRarityBit(it.jokerData.rarity));
          Item expect = wanted ? it.item : Item::RETRY;
          Item expectEd = wanted && editions ? it.jokerData.edition
                                             : Item::No_Edition;
          ShopJokerScan::Joker got = scan.next();
          checked++;
          jokersSeen += wanted;
          editionsSeen += expectEd != Item::No_Edition;
          if ((got.joker != expect || got.edition != expectEd) && wrong++ < 5)
            std::printf("    %s ante %d mask %u ed %d item %d: fast %s/%s, "
                        "full %s/%s\n",
                        seed.tostring().c_str(), ante, mask, editions, i,
                        itemToString(got.joker).c_str(),
                        itemToString(got.edition).c_str(),
                        itemToString(expect).c_str(),
                        itemToString(expectEd).c_str());
        }
      }
    }
    report(wrong == 0,
           std::string("shop items, ") + setupName[s] + " (" +
               std::to_string(jokersSeen) + " jokers, " +
               std::to_string(editionsSeen) + " editions)",
           checked, wrong);
  }
}

// findShopJoker() vs looping over nextShopItem(), for many jokers, antes,
// editions; then two calls in a row (the second continues the first).
static void testFind(int seeds) {
  const Item jokers[] = {Item::Blueprint,  Item::Brainstorm,  Item::Turtle_Bean,
                         Item::Burglar,    Item::Joker,       Item::Showman,
                         Item::DNA,        Item::Invisible_Joker};
  const Item editions[] = {Item::RETRY, Item::Negative, Item::Polychrome,
                           Item::No_Edition};
  long long checked = 0, wrong = 0, found = 0;
  Seed seed(std::string("FINDTST1"));
  for (int k = 0; k < seeds; k++, seed.next()) {
    Item joker = jokers[k % 8];
    Item ed = editions[(k / 8) % 4];
    int ante = 1 + k % 8, items = 1 + k % 60;
    Instance full(seed), fast(seed);
    full.initLocks(1, false, true);
    fast.initLocks(1, false, true);
    for (int call = 0; call < 2; call++) {
      int expect = -1;
      for (int i = 0; i < items; i++) {
        ShopItem it = full.nextShopItem(ante);
        if (it.item == joker &&
            (ed == Item::RETRY || it.jokerData.edition == ed)) {
          expect = i;
          break;
        }
      }
      int got = findShopJoker(fast, ante, joker, items, ed);
      checked++;
      found += expect >= 0;
      if (got != expect && wrong++ < 5)
        std::printf("    %s %s ante %d call %d: fast %d, full %d\n",
                    seed.tostring().c_str(), itemToString(joker).c_str(),
                    ante, call, got, expect);
    }
  }
  report(wrong == 0,
         "findShopJoker (" + std::to_string(found) + " found)", checked,
         wrong);

  // Jokers whose rarity differs between game versions (e.g. Vagabond:
  // uncommon in 1.0.0, rare in 1.0.1), searched under both versions.
  std::vector<Item> changed;
  for (int i = 0; i < (int)Item::ITEMS_END; i++)
    if (jokerRarity((Item)i, 10014) != jokerRarity((Item)i, 10103))
      changed.push_back((Item)i);
  checked = wrong = found = 0;
  Seed vs(std::string("VERSION1"));
  for (int k = 0; k < seeds / 4; k++, vs.next()) {
    Item joker = changed[k % changed.size()];
    long version = k % 2 ? 10014 : 10103;
    if (jokerRarity(joker, version) == Item::Legendary ||
        jokerRarity(joker, version) == Item::RETRY)
      continue; // not a shop joker in this version
    int ante = 1 + k % 8;
    Instance full(vs), fast(vs);
    full.params.version = fast.params.version = version;
    full.initLocks(1, false, true);
    fast.initLocks(1, false, true);
    int expect = -1;
    for (int i = 0; i < 60; i++)
      if (full.nextShopItem(ante).item == joker) { expect = i; break; }
    int got = findShopJoker(fast, ante, joker, 60);
    checked++;
    found += expect >= 0;
    if (got != expect && wrong++ < 5)
      std::printf("    %s %s v%ld: fast %d, full %d\n", vs.tostring().c_str(),
                  itemToString(joker).c_str(), version, got, expect);
  }
  report(wrong == 0 && !changed.empty(),
         "findShopJoker, " + std::to_string(changed.size()) +
             " jokers that changed rarity (" + std::to_string(found) + " found)",
         checked, wrong);

  // exact(): the same options again is fine, different ones are flagged.
  Seed s1(std::string("GUARD111"));
  Instance inst(s1);
  ShopJokerScan a(inst, 3, SHOP_UNCOMMON), b(inst, 3, SHOP_UNCOMMON),
      c(inst, 3, SHOP_UNCOMMON, true), d(inst, 3, SHOP_RARE),
      e(inst, 4, SHOP_RARE);
  bool ok = a.exact() && b.exact() && !c.exact() && !d.exact() && e.exact();
  report(ok, "exact() flags mixed options", 5, ok ? 0 : 1);
}

static bool fullHasSoul(Instance &inst, int size, int ante) {
  bool soul = false;
  for (Item it : inst.nextArcanaPack(size, ante)) soul |= it == Item::The_Soul;
  return soul;
}

static void testSouls(int seeds) {
  struct Case {
    const char *name;
    void (*prep)(Instance &);
  } cases[] = {
      {"run-start locks", [](Instance &i) { i.initLocks(1, false, true); }},
      {"no locks", [](Instance &) {}},
      {"The Soul locked", [](Instance &i) { i.lock(Item::The_Soul); }},
      {"a tarot locked (slow path)", [](Instance &i) { i.lock(Item::The_Fool); }},
      {"Omen Globe (slow path)",
       [](Instance &i) { i.activateVoucher(Item::Omen_Globe); }},
      {"Showman (slow path)", [](Instance &i) { i.params.showman = true; }},
  };
  for (auto &c : cases) {
    long long checked = 0, wrong = 0, souls = 0;
    Seed seed(std::string("SOULTST1"));
    for (int k = 0; k < seeds; k++, seed.next()) {
      for (int ante = 1; ante <= 2; ante++) {
        Instance full(seed), fast(seed);
        c.prep(full);
        c.prep(fast);
        // Several packs in a row, so the soul stream's later states and the
        // lock state after each pack are checked too.
        for (int p = 0; p < 4; p++) {
          int size = p % 2 ? 3 : 5;
          bool expect = fullHasSoul(full, size, ante);
          bool got = arcanaPackHasSoul(fast, size, ante);
          checked++;
          souls += expect;
          bool locksSame = full.isLocked(Item::The_Soul) ==
                           fast.isLocked(Item::The_Soul);
          if ((got != expect || !locksSame) && wrong++ < 5)
            std::printf("    %s ante %d pack %d: fast %d, full %d\n",
                        seed.tostring().c_str(), ante, p, got, expect);
        }
      }
    }
    report(wrong == 0,
           std::string("arcana souls, ") + c.name + " (" +
               std::to_string(souls) + " souls)",
           checked, wrong);
  }
}

// The original filter code for these checks (filters.cpp before fastscan),
// using the full generators. Called on an Instance prepared like
// brainstormFilter() prepares it.
static long originalTail(Instance inst, const FilterConfig &cfg) {
  inst.initLocks(1, false, true);
  if (cfg.souls > 0) {
    for (int i = 1; i <= cfg.souls; i++) {
      auto tarots = inst.nextArcanaPack(5, 1);
      bool found_soul = false, found_perkeo = false;
      for (int t = 0; t < 5; t++) {
        if (tarots[t] == Item::The_Soul) {
          found_soul = true;
          if (cfg.perkeo &&
              inst.nextJoker(ItemSource::Soul, 1, true).joker == Item::Perkeo)
            found_perkeo = true;
          break;
        }
      }
      if (!found_soul) return 0;
      if (cfg.perkeo && !found_perkeo) return 0;
    }
  }
  if (cfg.bean) {
    bool bean = false;
    for (int i = 0; i < 150; i++)
      if (inst.nextShopItem(8).item == Item::Turtle_Bean) { bean = true; break; }
    if (!bean) return 0;
  }
  if (cfg.burglar) {
    bool burglar = false;
    for (int i = 0; i < 50 && !burglar; i++)
      burglar = inst.nextShopItem(6).item == Item::Burglar;
    for (int i = 0; i < 100 && !burglar; i++)
      burglar = inst.nextShopItem(7).item == Item::Burglar;
    for (int i = 0; i < 100 && !burglar; i++)
      burglar = inst.nextShopItem(8).item == Item::Burglar;
    if (!burglar) return 0;
  }
  if (cfg.retcon) {
    bool directorsCut = false, retcon = false;
    for (int i = 1; i < 9; i++) {
      auto voucher = inst.nextVoucher(i);
      inst.activateVoucher(voucher);
      if (voucher == Item::Directors_Cut || directorsCut) {
        directorsCut = true;
        if (voucher == Item::Retcon) retcon = true;
      }
    }
    if (!retcon) return 0;
  }
  return 1;
}

static void testFilters(int seeds) {
  struct Named {
    const char *name;
    FilterConfig cfg;
  };
  std::vector<Named> cfgs;
  FilterConfig c;
  c = FilterConfig(); c.bean = true; cfgs.push_back({"bean", c});
  c = FilterConfig(); c.burglar = true; cfgs.push_back({"burglar", c});
  c = FilterConfig(); c.bean = c.burglar = true; cfgs.push_back({"bean+burglar", c});
  c = FilterConfig(); c.bean = c.burglar = c.retcon = true; cfgs.push_back({"bean+burglar+retcon", c});
  c = FilterConfig(); c.souls = 1; cfgs.push_back({"1 soul", c});
  c = FilterConfig(); c.souls = 2; cfgs.push_back({"2 souls", c});
  c = FilterConfig(); c.souls = 1; c.perkeo = true; cfgs.push_back({"soul+perkeo", c});
  c = FilterConfig(); c.souls = 1; c.bean = true; cfgs.push_back({"soul+bean", c});
  for (auto &nc : cfgs) {
    long long wrong = 0, hits = 0;
    Seed seed(std::string("FILTTST1"));
    Instance inst(seed);
    for (int k = 0; k < seeds; k++) {
      long expect = originalTail(inst, nc.cfg);
      long got = brainstormFilter(inst, nc.cfg);
      hits += expect >= 1;
      if ((got >= 1) != (expect >= 1) && wrong++ < 5)
        std::printf("    %s: fast %ld, original %ld\n",
                    seed.tostring().c_str(), got, expect);
      inst.next();
    }
    report(wrong == 0,
           std::string("filter ") + nc.name + " (" + std::to_string(hits) +
               " pass)",
           seeds, wrong);
  }
}

int main(int argc, char **argv) {
  bool quick = argc > 1 && std::string(argv[1]) == "quick";
  testItems(quick ? 40 : 400, 150);
  testFind(quick ? 20000 : 200000);
  testSouls(quick ? 20000 : 300000);
  testFilters(quick ? 5000 : 60000);
  std::printf("\n%s (%d failing check%s)\n", failures ? "FAILED" : "ALL PASSED",
              failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
