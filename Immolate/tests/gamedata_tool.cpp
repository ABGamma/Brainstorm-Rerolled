// gamedata_tool — helper for testing the Lua exporter
// (Brainstorm/Core/gamedata.lua) without the game; see tools/test-exporter.sh.
//
//   gamedata_tool dump          prints the built-in (unmodded) game data
//   gamedata_tool check FILE    reads game data text from FILE and compares
//                               it with the built-in data: lists must be
//                               equal, roll boundaries equal to 1e-9
//                               (the exporter finds them by probing).

#include "filterprog.hpp"
#include "gamedata.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

static int differences = 0;

static void differ(const std::string &what) {
  if (differences++ < 20) std::printf("  differs: %s\n", what.c_str());
}

static std::string listText(const GameData &d, const GameData::List &list) {
  std::string out;
  for (const GameData::Entry &e : list)
    out += d.keyOf(e.id) + (e.state == GameData::Entry::GATED ? "?" : e.state == GameData::Entry::UNAVAILABLE ? "!" : "") + " ";
  return out;
}

static void compareSteps(const std::string &name, const GameData &a,
                         const GameData::Steps &x, const GameData &b,
                         const GameData::Steps &y) {
  if (x.from.size() != y.from.size()) return differ(name + ": number of steps");
  for (size_t i = 0; i < x.from.size(); i++) {
    if (std::fabs(x.from[i] - y.from[i]) > 1e-9)
      differ(name + " step " + std::to_string(i) + ": boundary");
    if (name != "rarity" && a.keyOf(x.value[i]) != b.keyOf(y.value[i]))
      differ(name + " step " + std::to_string(i) + ": " + a.keyOf(x.value[i]) + " vs " + b.keyOf(y.value[i]));
  }
}

int main(int argc, char **argv) {
  const GameData &v = *GameData::vanilla();
  if (argc == 2 && std::string(argv[1]) == "dump") {
    std::fputs(v.toText().c_str(), stdout);
    return 0;
  }
  // describe FILE SEED [ANTES CARDS PACKS]: what the library predicts for
  // a run on SEED in the game described by FILE ("-" = the unmodded game).
  if (argc >= 4 && std::string(argv[1]) == "describe") {
    std::shared_ptr<const GameData> data = GameData::vanilla();
    if (std::string(argv[2]) != "-") {
      std::ifstream in(argv[2]);
      std::stringstream text;
      text << in.rdbuf();
      std::string err;
      data = GameData::parse(text.str(), err);
      if (!data) {
        std::printf("FAIL  %s\n", err.c_str());
        return 1;
      }
    }
    Seed seed{std::string(argv[3])};
    int antes = argc > 4 ? std::atoi(argv[4]) : 1;
    int cards = argc > 5 ? std::atoi(argv[5]) : 4;
    int packs = argc > 6 ? std::atoi(argv[6]) : 4;
    std::fputs(FilterProgram::describe(data, seed, antes, cards, packs).c_str(), stdout);
    return 0;
  }
  // filter 'TEXT' [SEED]: reads a filter (unmodded game) and shows what the
  // parser made of it, field by field, or its error; with a seed, whether
  // the seed matches. For trying things out while learning the code.
  if (argc >= 3 && std::string(argv[1]) == "filter") {
    std::string err;
    auto prog = FilterProgram::parse(argv[2], err);
    if (!prog) {
      std::printf("REJECTED: %s\n", err.c_str());
      return 1;
    }
    const char *kinds[] = {"TAG", "VOUCHER", "PACK", "JOKER", "SOUL", "BOSS"};
    const GameData &d = prog->data();
    int n = 0;
    for (const Condition &c : prog->conditions()) {
      std::printf("condition %d: kind=%s id=%s ante=%d anteTo=%d within=%d blind=%d "
                  "anyEdition=%s editionId=%s packs=%d size=%d spectral=%s\n",
                  ++n, kinds[c.kind], c.id == GameData::NONE ? "NONE" : d.keyOf(c.id).c_str(),
                  c.ante, c.anteTo, c.within, c.blind, c.anyEdition ? "true" : "false",
                  c.editionId == GameData::NONE ? "NONE" : d.keyOf(c.editionId).c_str(),
                  c.packs, c.size, c.spectral ? "true" : "false");
    }
    if (argc >= 4) {
      Seed seed{std::string(argv[3])};
      std::printf("seed %s: %s\n", argv[3], prog->matches(seed) ? "MATCHES" : "does not match");
    }
    return 0;
  }
  if (argc != 3 || std::string(argv[1]) != "check") {
    std::fprintf(stderr, "usage: gamedata_tool dump | check FILE | describe FILE SEED [ANTES CARDS PACKS]\n");
    return 2;
  }
  std::ifstream in(argv[2]);
  std::stringstream text;
  text << in.rdbuf();
  std::string err;
  auto parsed = GameData::parse(text.str(), err);
  if (!parsed) {
    std::printf("FAIL  %s\n", err.c_str());
    return 1;
  }
  const GameData &d = *parsed;
  if (d.pad != v.pad) differ("pad");
  for (int a = 1; a <= GameData::MAX_ANTE; a++)
    if (listText(d, d.tags[a]) != listText(v, v.tags[a])) differ("tags " + std::to_string(a));
  if (d.vouchers.size() != v.vouchers.size()) differ("number of vouchers");
  else
    for (size_t i = 0; i < v.vouchers.size(); i++) {
      const GameData::Voucher &x = d.vouchers[i], &y = v.vouchers[i];
      bool same = d.keyOf(x.id) == v.keyOf(y.id) && x.start == y.start &&
                  x.later == y.later && x.needs.size() == y.needs.size();
      for (size_t k = 0; same && k < x.needs.size(); k++)
        same = d.keyOf(x.needs[k]) == v.keyOf(y.needs[k]);
      if (!same) differ("voucher " + v.keyOf(y.id));
    }
  if (d.keyOf(d.firstPack) != v.keyOf(v.firstPack)) differ("firstpack");
  compareSteps("pack", d, d.packs, v, v.packs);
  if (std::fabs(d.shopJoker / d.shopTotal - v.shopJoker / v.shopTotal) > 1e-12 ||
      d.shopSlots != v.shopSlots)
    differ("shop");
  if (d.rarityPolls != v.rarityPolls) differ("raritypolls");
  compareSteps("rarity", d, d.rarities, v, v.rarities);
  if (d.rarityPools.size() != v.rarityPools.size()) differ("number of rarities");
  else
    for (size_t i = 0; i < v.rarityPools.size(); i++) {
      const GameData::RarityPool &x = d.rarityPools[d.rarities.value[i]],
                                 &y = v.rarityPools[v.rarities.value[i]];
      if (x.stream != y.stream) differ("rarity stream " + y.stream + " vs " + x.stream);
      if (listText(d, x.jokers) != listText(v, y.jokers)) differ("jokers of " + y.stream);
    }
  compareSteps("edition", d, d.editions, v, v.editions);
  if (d.soul != v.soul) differ("soul");
  if (d.blackHole != v.blackHole) differ("blackhole");
  if (d.legendaryStream != v.legendaryStream ||
      listText(d, d.legendaries) != listText(v, v.legendaries))
    differ("legendary");
  if (d.bosses.size() != v.bosses.size()) differ("number of bosses");
  else
    for (size_t i = 0; i < v.bosses.size(); i++)
      if (d.keyOf(d.bosses[i].id) != v.keyOf(v.bosses[i].id) || d.bosses[i].antes != v.bosses[i].antes)
        differ("boss " + v.keyOf(v.bosses[i].id));
  std::printf("%s  exported game data == built-in data (%d difference%s)\n",
              differences ? "FAIL" : "PASS", differences, differences == 1 ? "" : "s");
  return differences ? 1 : 0;
}
