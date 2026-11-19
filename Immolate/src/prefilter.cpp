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
  const int NONE = GameData::NONE;
  data = cfg.program ? cfg.program->dataPtr() : GameData::vanilla();
  const GameData &d = *data;
  // A weighted game picks by weight from shared streams (gamedata.hpp):
  // no "first roll of a list" to screen on.
  if (d.weighted) return;

  // Ante 1's lists as a fresh run has them. If the game drops unavailable
  // items instead of keeping their slots, there is nothing to re-roll.
  for (const GameData::Entry &e : d.tags[1]) {
    int id = e.state == GameData::Entry::AVAILABLE ? e.id : NONE;
    if (d.pad || id != NONE) tagList.push_back(id);
  }
  for (const GameData::Voucher &v : d.vouchers)
    if (d.pad || v.start) voucherList.push_back(v.start ? v.id : NONE);

  if (cfg.custom == customFilters::NO_FILTER) {
    // The menu options (unmodded game only: they are checked with
    // Immolate's built-in lists).
    if (data == GameData::vanilla()) {
      auto idOf = [&](Item item) { return d.idOf(keyFromItem(item)); };
      if (cfg.tag != Item::RETRY) checks.push_back({TAG, idOf(cfg.tag)});
      if (cfg.observatory) checks.push_back({VOUCHER, idOf(Item::Telescope)});
      else if (cfg.voucher != Item::RETRY) checks.push_back({VOUCHER, idOf(cfg.voucher)});
      if (cfg.pack != Item::RETRY) checks.push_back({PACK, idOf(cfg.pack)});
    }
    // The custom filter's ante 1 conditions that are exactly a first roll
    // (filterprog.hpp): the Small Blind tag, the voucher, and the first
    // random pack: "one of the first 2 packs" when the first is fixed and
    // is a different one (or "the first pack" when none is fixed).
    if (cfg.program)
      for (const Condition &c : cfg.program->conditions()) {
        if (c.ante != 1 || c.anteTo != 1) continue; // ranges: no
        if (c.kind == Condition::TAG && c.blind == 1)
          checks.push_back({TAG, c.id});
        if (c.kind == Condition::VOUCHER) checks.push_back({VOUCHER, c.id});
        if (c.kind == Condition::PACK &&
            (d.firstPack != NONE ? c.within == 2 && c.id != d.firstPack
                                 : c.within == 1))
          checks.push_back({PACK, c.id});
      }
    // A kind with nothing to pick from can't be screened.
    std::vector<Check> usable;
    for (const Check &c : checks) {
      bool ok = c.want != NONE &&
                (c.kind == TAG ? !tagList.empty()
                 : c.kind == VOUCHER ? !voucherList.empty()
                                     : !d.packs.value.empty());
      if (ok) usable.push_back(c);
    }
    checks = usable;
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
                     int count, int items[LANES], bool decided[LANES]) {
  const int NONE = GameData::NONE;
  double v[LANES];
  streamStart(keyFor(kind), lanes, idx, count, v);

  if (kind == PACK) { // one roll picks the pack; nothing to re-roll
    const GameData::Steps &packs = data->packs;
    for (int j = 0; j < count; j++) {
      int step = luaRandomDecide(v[j], [&](double x) { return packs.index(x); });
      items[idx[j]] = packs.value[step];
      decided[idx[j]] = true;
    }
    return;
  }

  // The game's pick: item number N of the list; if that slot is unavailable,
  // again with "<key>_resample2", "_resample3", ... (see pick() in
  // filterprog.cpp, which the full check uses).
  const std::vector<int> &list = kind == TAG ? tagList : voucherList;
  auto pick = [&](double seedValue) {
    return list[luaRandint(seedValue, 0, (int)list.size() - 1)];
  };
  int pending[LANES], numPending = 0;
  for (int j = 0; j < count; j++) {
    int it = pick(v[j]);
    items[idx[j]] = it;
    decided[idx[j]] = it != NONE;
    if (!decided[idx[j]]) pending[numPending++] = idx[j];
  }
  for (int resample = 2; resample <= MAX_RESAMPLE && numPending > 0;
       resample++) {
    streamStart(resampleKey(kind, resample), lanes, pending, numPending, v);
    int still = 0;
    for (int j = 0; j < numPending; j++) {
      int it = pick(v[j]);
      items[pending[j]] = it;
      if (it == NONE) pending[still++] = pending[j];
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
  int idx[LANES], ids[LANES];
  for (int i = 0; i < n; i++) idx[i] = i;
  roll(kind, lanes, idx, n, ids, decided);
  for (int i = 0; i < n; i++) items[i] = itemFromGameKey(data->keyOf(ids[i]));
}

void Prefilter::run(Seed &seed, int n, bool maybe[LANES]) {
  LaneSeed lanes[LANES];
  collect(seed, n, lanes);
  int idx[LANES], count = n;
  for (int i = 0; i < n; i++) {
    maybe[i] = true;
    idx[i] = i;
  }
  int items[LANES];
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
