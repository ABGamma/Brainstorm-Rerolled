// prefilter.cpp — see prefilter.hpp.

#include "prefilter.hpp"

#include "filterprog.hpp"
#include "functions.hpp"

#include <algorithm>
#include "rng.hpp"

#include <cstdint>

namespace {

// Stream keys for ante 1, as built by nextTag/nextVoucher/nextPack:
// RandomType::Tags + "1", RandomType::Voucher + "1", RandomType::Shop_Pack + "1".
const std::string &keyFor(Prefilter::Kind k) {
  static const std::string tag = RandomType::Tags + anteToString(1);
  static const std::string voucher = RandomType::Voucher + anteToString(1);
  static const std::string pack = RandomType::Shop_Pack + anteToString(1);
  return k == Prefilter::TAG ? tag : k == Prefilter::VOUCHER ? voucher : pack;
}

// Re-roll keys exactly as Instance::randchoice builds them:
// ID + "_resample" + anteToString(resample). Built once, not per seed.
const std::string &resampleKey(Prefilter::Kind k, int resample) {
  static const auto table = [] {
    std::vector<std::vector<std::string>> t(3);
    for (int kind = 0; kind < 3; kind++)
      for (int r = 0; r <= Prefilter::MAX_RESAMPLE; r++)
        t[kind].push_back(keyFor((Prefilter::Kind)kind) + "_resample" +
                          anteToString(r));
    return t;
  }();
  return table[k][resample];
}

} // namespace

Prefilter::Prefilter(const FilterConfig &cfg) {
  // Locks exactly as brainstormFilter() sets them up for every seed.
  Seed dummy(std::string("1"));
  Instance inst(dummy);
  inst.initLocks(1, false, true);
  locked.assign(inst.locked, inst.locked + (int)Item::ITEMS_END);

  if (cfg.custom == customFilters::NO_FILTER) {
    // The menu options.
    if (cfg.tag != Item::RETRY) checks.push_back({TAG, cfg.tag});
    if (cfg.observatory) checks.push_back({VOUCHER, Item::Telescope});
    else if (cfg.voucher != Item::RETRY) checks.push_back({VOUCHER, cfg.voucher});
    if (cfg.pack != Item::RETRY) checks.push_back({PACK, cfg.pack});
    // The custom filter's ante 1 conditions that are exactly a first roll
    // (filterprog.hpp): the Small Blind tag, the voucher, and "one of the
    // first 2 packs" when it isn't the Buffoon Pack that always comes first
    // (so it must be the 2nd pack = the first random one).
    if (cfg.program)
      for (const Condition &c : cfg.program->conditions()) {
        if (c.ante != 1 || c.anteTo != 1) continue; // ranges: no
        if (c.kind == Condition::TAG && c.blind == 1)
          checks.push_back({TAG, c.item});
        if (c.kind == Condition::VOUCHER) checks.push_back({VOUCHER, c.item});
        if (c.kind == Condition::PACK && c.within == 2 &&
            c.item != Item::Buffoon_Pack)
          checks.push_back({PACK, c.item});
      }
    // Cheapest key first: "Tag1" (4 chars), "Voucher1" (8), "shop_pack1" (10).
    std::stable_sort(checks.begin(), checks.end(),
                     [](const Check &a, const Check &b) { return a.kind < b.kind; });
  }
  // firstRolls() (tests) may ask for any kind, so always know every length;
  // collect() only computes the ones in use.
  for (Kind k : {TAG, VOUCHER, PACK}) {
    bool used = false;
    for (const Check &c : checks) used |= c.kind == k;
    if (!used && active()) continue;
    keyLengths.push_back((int)keyFor(k).size());
    if (k != PACK) keyLengths.push_back((int)resampleKey(k, 2).size());
  }
}

void Prefilter::collect(Seed &seed, int n, LaneSeed lanes[LANES]) {
  for (int i = 0; i < n; i++) {
    lanes[i].hashed = seed.pseudohash(0);
    // Seed::pseudohash caches partial results, so these are cheap.
    for (int len : keyLengths) lanes[i].prefix[len] = seed.pseudohash(len);
    seed.next();
  }
}

void Prefilter::streamStart(const std::string &key,
                            const LaneSeed lanes[LANES], const int *idx,
                            int count, double out[LANES]) {
  const size_t len = key.size();
  // pseudohash_from() for `count` seeds at once: the same operations in the
  // same order, but the inner loop over seeds is independent work the CPU
  // can overlap, instead of one long chain of dependent divisions.
  double num[LANES];
  for (int j = 0; j < count; j++) num[j] = lanes[idx[j]].prefix[len];
  for (size_t i = len; i > 0; i--) {
    double c = (double)key[i - 1];
    double posTerm = 3.141592653589793116 * (double)i;
    for (int j = 0; j < count; j++)
      num[j] = fractFast(1.1239285023 / num[j] * c * 3.141592653589793116 +
                         posTerm);
  }
  // First step of the stream, as Instance::get_node does on first use.
  for (int j = 0; j < count; j++) {
    double node = Instance::advance_node(num[j]);
    out[j] = (node + lanes[idx[j]].hashed) / 2;
  }
}

void Prefilter::roll(Kind kind, const LaneSeed lanes[LANES], const int *idx,
                     int count, Item items[LANES], bool decided[LANES]) {
  double v[LANES];
  streamStart(keyFor(kind), lanes, idx, count, v);

  if (kind == PACK) { // weighted choice, no locks involved
    for (int j = 0; j < count; j++) {
      items[idx[j]] = PACKS[weightedIndex(v[j], PACKS)].item;
      decided[idx[j]] = true;
    }
    return;
  }

  // Instance::randchoice: pick; if the pick is locked (or RETRY), re-roll
  // with "<key>_resample2", "_resample3", ... until an unlocked item comes up.
  auto pick = [&](double seedValue) {
    return kind == TAG ? TAGS[luaRandint(seedValue, 0, (int)TAGS.size() - 1)]
                       : VOUCHERS[luaRandint(seedValue, 0, (int)VOUCHERS.size() - 1)];
  };
  auto needsReroll = [&](Item it) {
    return it == Item::RETRY || locked[(int)it];
  };
  int pending[LANES], numPending = 0;
  for (int j = 0; j < count; j++) {
    Item it = pick(v[j]);
    items[idx[j]] = it;
    decided[idx[j]] = !needsReroll(it);
    if (!decided[idx[j]]) pending[numPending++] = idx[j];
  }
  for (int resample = 2; resample <= MAX_RESAMPLE && numPending > 0;
       resample++) {
    streamStart(resampleKey(kind, resample), lanes, pending, numPending, v);
    int still = 0;
    for (int j = 0; j < numPending; j++) {
      Item it = pick(v[j]);
      items[pending[j]] = it;
      if (needsReroll(it)) pending[still++] = pending[j];
      else decided[pending[j]] = true;
    }
    numPending = still;
  }
  // Anything left (re-rolled MAX_RESAMPLE times) stays undecided.
}

void Prefilter::firstRolls(Kind kind, Seed &seed, int n, Item items[LANES],
                           bool decided[LANES]) {
  LaneSeed lanes[LANES];
  collect(seed, n, lanes);
  int idx[LANES];
  for (int i = 0; i < n; i++) idx[i] = i;
  roll(kind, lanes, idx, n, items, decided);
}

void Prefilter::run(Seed &seed, int n, bool maybe[LANES]) {
  LaneSeed lanes[LANES];
  collect(seed, n, lanes);
  int idx[LANES], count = n;
  for (int i = 0; i < n; i++) {
    maybe[i] = true;
    idx[i] = i;
  }
  Item items[LANES];
  bool decided[LANES];
  for (const Check &c : checks) {
    roll(c.kind, lanes, idx, count, items, decided);
    // Keep only seeds that can still pass.
    int kept = 0;
    for (int j = 0; j < count; j++) {
      int lane = idx[j];
      if (decided[lane] && items[lane] != c.want) maybe[lane] = false;
      else idx[kept++] = lane;
    }
    count = kept;
    if (count == 0) break;
  }
}
