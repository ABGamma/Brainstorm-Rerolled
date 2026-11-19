// gamedata.cpp — see gamedata.hpp.

#include "gamedata.hpp"

#include "filterprog.hpp" // gameKeyOf
#include "filters.hpp"
#include "functions.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

int GameData::idOf(const std::string &key) const {
  auto it = ids.find(key);
  return it == ids.end() ? NONE : it->second;
}

const std::string &GameData::keyOf(int id) const {
  static const std::string none;
  return id >= 0 && id < (int)keys.size() ? keys[id] : none;
}

int GameData::intern(const std::string &key) {
  auto it = ids.find(key);
  if (it != ids.end()) return it->second;
  keys.push_back(key);
  ids[key] = (int)keys.size() - 1;
  return (int)keys.size() - 1;
}

const char *const GameData::CARD_SET_NAMES[GameData::CARD_SETS] = {"Tarot", "Planet", "Spectral"};

// ---------------------------------------------------------------------------
// The unmodded game, from the tables in items.hpp

struct VanillaBuilder {
  GameData &d;

  // The smallest roll that is > threshold: the engine tests "roll > 0.7",
  // a Steps entry means "roll >= from".
  static double above(double threshold) { return std::nextafter(threshold, 2.0); }

  GameData::Entry entry(Item item, bool locked, bool gated = false) {
    GameData::Entry e;
    if (item == Item::RETRY) {
      e.state = GameData::Entry::UNAVAILABLE;
      return e;
    }
    e.id = d.intern(gameKeyOf(item));
    e.state = !locked ? GameData::Entry::AVAILABLE
              : gated ? GameData::Entry::GATED
                      : GameData::Entry::UNAVAILABLE;
    return e;
  }

  template <class Pool>
  void jokers(const std::string &stream, const Pool &pool, Instance &inst) {
    static const Item GATED[] = {Item::Steel_Joker, Item::Stone_Joker,
                                 Item::Lucky_Cat, Item::Golden_Ticket,
                                 Item::Glass_Joker};
    GameData::RarityPool p;
    p.stream = stream;
    for (Item j : pool) {
      bool gated = false;
      for (Item g : GATED) gated |= g == j;
      p.jokers.push_back(entry(j, inst.isLocked(j), gated));
    }
    d.rarityPools.push_back(p);
  }

  void build() {
    Seed dummy(std::string("1"));

    // Run-start locks, as brainstormFilter() has always used them.
    Instance inst(dummy);
    inst.initLocks(1, false, true);

    // Tags: more unlock from ante 2 on.
    {
      Instance tagInst(dummy);
      tagInst.initLocks(1, false, true);
      for (int a = 1; a <= GameData::MAX_ANTE; a++) {
        if (a >= 2) tagInst.initUnlocks(a, false);
        for (Item t : TAGS) d.tags[a].push_back(entry(t, tagInst.isLocked(t)));
      }
    }

    // Vouchers come in pairs: the upgrade needs its base voucher.
    for (size_t i = 0; i < VOUCHERS.size(); i++) {
      GameData::Voucher v;
      v.id = d.intern(gameKeyOf(VOUCHERS[i]));
      v.start = !inst.isLocked(VOUCHERS[i]);
      v.later = true;
      if (i % 2 == 1) v.needs.push_back(d.intern(gameKeyOf(VOUCHERS[i - 1])));
      d.vouchers.push_back(v);
    }

    // Packs: PACKS[0] holds the total weight (Instance::randweightedchoice).
    d.firstPack = d.intern(gameKeyOf(Item::Buffoon_Pack));
    double total = PACKS[0].weight, cume = 0;
    for (size_t i = 1; i < PACKS.size(); i++) {
      d.packs.from.push_back(cume / total);
      d.packs.value.push_back(d.intern(gameKeyOf(PACKS[i].item)));
      cume += PACKS[i].weight;
    }

    // Shop: Instance::getShopInstance() on a plain deck, no vouchers.
    ShopInstance shop = inst.getShopInstance();
    d.shopTotal = shop.getTotalRate();
    d.shopJoker = shop.jokerRate;
    d.shopTarot = shop.tarotRate;
    d.shopPlanet = shop.planetRate;
    d.shopPlaying = shop.playingCardRate;
    d.shopSpectral = shop.spectralRate;
    d.shopSlots = 2;

    // Consumables, in the game's order. Planet X, Ceres and Eris need their
    // poker hand played first; The Soul and Black Hole (the last two
    // Spectral slots, RETRY in SPECTRALS) never come from the list.
    for (Item t : TAROTS) d.cards[0].push_back(entry(t, inst.isLocked(t)));
    for (Item p : PLANETS) d.cards[1].push_back(entry(p, inst.isLocked(p)));
    for (size_t i = 0; i < SPECTRALS.size(); i++) {
      if (SPECTRALS[i] != Item::RETRY) {
        d.cards[2].push_back(entry(SPECTRALS[i], inst.isLocked(SPECTRALS[i])));
      } else {
        GameData::Entry e;
        e.id = d.intern(i + 2 == SPECTRALS.size() ? "c_soul" : "c_black_hole");
        e.state = GameData::Entry::UNAVAILABLE;
        d.cards[2].push_back(e);
      }
    }

    // Instance::nextJoker: rarity > 0.95 Rare, > 0.7 Uncommon, else Common.
    d.rarityPolls = 1;
    d.rarities.from = {0, above(0.7), above(0.95)};
    d.rarities.value = {0, 1, 2};
    jokers(RandomType::Joker_Common + ItemSource::Shop, COMMON_JOKERS, inst);
    jokers(RandomType::Joker_Uncommon + ItemSource::Shop, UNCOMMON_JOKERS, inst);
    jokers(RandomType::Joker_Rare + ItemSource::Shop, RARE_JOKERS, inst);

    // Instance::nextJoker's edition thresholds, same expressions.
    int editionRate = 1;
    d.editions.from = {0, above(1 - 0.04 * editionRate),
                       above(1 - 0.02 * editionRate),
                       above(1 - 0.006 * editionRate), above(0.997)};
    d.editions.value = {GameData::NONE, d.intern("e_foil"), d.intern("e_holo"),
                        d.intern("e_polychrome"), d.intern("e_negative")};

    d.soul = true;
    d.legendaryStream = RandomType::Joker_Legendary;
    for (Item j : LEGENDARY_JOKERS) d.legendaries.push_back(entry(j, false));

    // Instance::nextBoss / the game's get_new_boss(): showdown bosses on
    // ante 8, the others from their minimum ante on (never on ante 8).
    for (size_t i = 0; i < BOSSES.size(); i++) {
      GameData::Boss b;
      b.id = d.intern(gameKeyOf(BOSSES[i]));
      for (int a = 1; a <= GameData::MAX_ANTE; a++)
        if (bossPossible(BOSSES[i], a)) b.antes |= 1u << a;
      d.bosses.push_back(b);
    }
  }
};

std::shared_ptr<const GameData> GameData::vanilla() {
  static const std::shared_ptr<const GameData> data = [] {
    auto d = std::make_shared<GameData>();
    VanillaBuilder{*d}.build();
    return d;
  }();
  return data;
}

// ---------------------------------------------------------------------------
// Text

namespace {

bool parseNumber(const std::string &s, double &out) {
  if (s.empty()) return false;
  char *end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end && *end == '\0' && std::isfinite(out);
}

bool parseCount(const std::string &s, int lo, int hi, int &out) {
  double v;
  if (!parseNumber(s, v) || v != std::floor(v) || v < lo || v > hi) return false;
  out = (int)v;
  return true;
}

std::string number(double v) {
  char buf[40];
  std::snprintf(buf, sizeof buf, "%.17g", v);
  return buf;
}

// A new step must start above the previous one (the first at exactly 0).
bool addStep(GameData::Steps &steps, double from, int value) {
  if (!(from >= 0 && from < 1)) return false;
  if (steps.from.empty() ? from != 0 : from <= steps.from.back()) return false;
  steps.from.push_back(from);
  steps.value.push_back(value);
  return true;
}

std::string enc(const std::string &w) { return GameData::encodeWord(w); }

// "*<number>": the weight of the list entries after it (weighted games).
std::string weightWord(double w) { return "*" + number(w); }

void writeList(std::string &out, const GameData &d, const GameData::List &list) {
  double weight = GameData::DEFAULT_WEIGHT;
  for (const GameData::Entry &e : list) {
    if (d.weighted && e.weight != weight) {
      weight = e.weight;
      out += ' ' + weightWord(weight);
    }
    out += ' ';
    if (e.id == GameData::NONE) {
      out += '-';
      continue;
    }
    out += enc(d.keyOf(e.id));
    if (e.state == GameData::Entry::GATED) out += '?';
    if (e.state == GameData::Entry::UNAVAILABLE) out += '!';
  }
}

// A "*<number>" word (checked on the word as written, before decoding, so
// a key can never be taken for one: the exporter encodes a leading '*').
bool isWeightWord(const std::string &raw, double &weight) {
  return raw.size() >= 2 && raw[0] == '*' && parseNumber(raw.substr(1), weight) &&
         weight >= 0;
}

} // namespace

bool GameData::parseList(const std::vector<std::string> &words,
                         const std::vector<std::string> &raw, size_t first,
                         List &out) {
  double weight = DEFAULT_WEIGHT;
  for (size_t i = first; i < words.size(); i++) {
    if (isWeightWord(raw[i], weight)) continue;
    std::string w = words[i];
    Entry e;
    e.weight = weight;
    if (w == "-") {
      e.state = Entry::UNAVAILABLE;
    } else {
      if (w.back() == '?') e.state = Entry::GATED, w.pop_back();
      else if (w.back() == '!') e.state = Entry::UNAVAILABLE, w.pop_back();
      if (w.empty()) return false;
      e.id = intern(w);
    }
    out.push_back(e);
  }
  return true;
}

std::shared_ptr<const GameData> GameData::parse(const std::string &text,
                                                std::string &error) {
  auto d = std::make_shared<GameData>();
  // Everything the vanilla defaults would otherwise leave filled in.
  d->shopTotal = d->shopJoker = 0;
  d->soul = false;
  d->editions.from.push_back(0);
  d->editions.value.push_back(NONE);

  std::istringstream lines(text);
  std::string line;
  int lineNo = 0;
  bool header = false;
  auto fail = [&](const std::string &what) {
    error = "game data, line " + std::to_string(lineNo) + ": " + what;
    return nullptr;
  };
  while (std::getline(lines, line)) {
    lineNo++;
    std::istringstream in(line);
    std::vector<std::string> w, raw;
    for (std::string s; in >> s;) raw.push_back(s), w.push_back(decodeWord(s));
    if (w.empty() || w[0][0] == '#') continue;
    const std::string &kind = w[0];
    size_t n = w.size();
    if (!header) {
      if (kind != "gamedata" || n != 2 || w[1] != "1")
        return fail("expected 'gamedata 1' (unsupported game data version?)");
      header = true;
      continue;
    }
    int count = 0;
    double from = 0;
    if (kind == "pad" && n == 2 && parseCount(w[1], 0, 1, count)) {
      d->pad = count == 1;
    } else if (kind == "tags" && n >= 2 && parseCount(w[1], 1, MAX_ANTE, count)) {
      d->tags[count].clear();
      if (!d->parseList(w, raw, 2, d->tags[count])) return fail("bad list");
    } else if (kind == "voucher" && n >= 4) {
      Voucher v;
      int start = 0, later = 0;
      if (!parseCount(w[2], 0, 1, start) || !parseCount(w[3], 0, 1, later))
        return fail("expected: voucher <key> <0|1> <0|1> [required keys]");
      v.id = d->intern(w[1]);
      v.start = start == 1;
      v.later = later == 1;
      for (size_t i = 4; i < n; i++)
        if (!isWeightWord(raw[i], v.weight)) v.needs.push_back(d->intern(w[i]));
      d->vouchers.push_back(v);
    } else if (kind == "firstpack" && n == 2) {
      d->firstPack = d->intern(w[1]);
    } else if (kind == "pack" && n == 3 && parseNumber(w[1], from)) {
      if (!addStep(d->packs, from, d->intern(w[2])))
        return fail("pack steps must start at 0 and rise");
    } else if (kind == "shop" && n == 4 && parseNumber(w[1], d->shopTotal) &&
               parseNumber(w[2], d->shopJoker) &&
               parseCount(w[3], 1, 8, d->shopSlots)) {
      if (d->shopTotal <= 0 || d->shopJoker < 0 || d->shopJoker > d->shopTotal)
        return fail("bad shop rates");
    } else if (kind == "shoprates" && n == 5 && parseNumber(w[1], d->shopTarot) &&
               parseNumber(w[2], d->shopPlanet) && parseNumber(w[3], d->shopPlaying) &&
               parseNumber(w[4], d->shopSpectral)) {
      if (d->shopTarot < 0 || d->shopPlanet < 0 || d->shopPlaying < 0 || d->shopSpectral < 0)
        return fail("bad shop rates");
    } else if (kind == "cards" && n >= 2) {
      int set = -1;
      for (int i = 0; i < CARD_SETS; i++)
        if (w[1] == CARD_SET_NAMES[i]) set = i;
      if (set < 0) return fail("expected: cards <Tarot|Planet|Spectral> <list>");
      d->cards[set].clear();
      if (!d->parseList(w, raw, 2, d->cards[set])) return fail("bad list");
    } else if (kind == "modsoul" && n >= 5 && parseNumber(w[2], from) &&
               parseCount(w[3], 0, 1, count)) {
      ModSoul m;
      m.id = d->intern(w[1]);
      m.rate = from;
      m.repeat = count == 1;
      for (size_t i = 4; i < n; i++) {
        bool known = false;
        for (int s = 0; s < CARD_SETS; s++)
          if (w[i] == CARD_SET_NAMES[s]) m.sets[s] = known = true;
        if (!known) return fail("expected: modsoul <key> <rate> <0|1> <Tarot|Planet|Spectral>...");
      }
      if (m.rate < 0 || m.rate > 1) return fail("bad modsoul rate");
      d->modSouls.push_back(m);
    } else if (kind == "raritypolls" && n == 2 &&
               parseCount(w[1], 1, 4, d->rarityPolls)) {
    } else if (kind == "rarity" && n >= 3 && parseNumber(w[1], from)) {
      if (!addStep(d->rarities, from, (int)d->rarityPools.size()))
        return fail("rarity steps must start at 0 and rise");
      RarityPool p;
      p.stream = w[2];
      if (!d->parseList(w, raw, 3, p.jokers)) return fail("bad list");
      d->rarityPools.push_back(p);
    } else if (kind == "edition" && n == 3 && parseNumber(w[1], from)) {
      // "-" = no edition (only needed for a gap between two editions).
      if (from <= 0 || !addStep(d->editions, from, w[2] == "-" ? NONE : d->intern(w[2])))
        return fail("edition steps must be above 0 and rise");
    } else if (kind == "soul" && n == 2 && parseCount(w[1], 0, 1, count)) {
      d->soul = count == 1;
    } else if (kind == "blackhole" && n == 2 && parseCount(w[1], 0, 1, count)) {
      d->blackHole = count == 1;
    } else if (kind == "legendary" && n >= 2) {
      d->legendaryStream = w[1];
      d->legendaries.clear();
      if (!d->parseList(w, raw, 2, d->legendaries)) return fail("bad list");
    } else if (kind == "editionstream" && n == 2) {
      d->editionStream = w[1];
    } else if (kind == "weights" && n == 2 && parseCount(w[1], 0, 1, count)) {
      d->weighted = count == 1;
    } else if (kind == "streams" && n == 4) {
      d->typeStream = w[1], d->shopStream = w[2], d->soulStream = w[3];
    } else if (kind == "vouchersperante" && n == 2 &&
               parseCount(w[1], 1, 8, d->vouchersPerAnte)) {
    } else if (kind == "packlist") {
      d->packList.clear();
      if (!d->parseList(w, raw, 1, d->packList)) return fail("bad list");
    } else if (kind == "shoptype" && n >= 3) {
      ShopType t;
      t.name = w[1];
      t.joker = t.name == "Joker";
      if (!parseNumber(w[2], t.rate) || t.rate < 0)
        return fail("expected: shoptype <name> <rate> [items]");
      if (!d->parseList(w, raw, 3, t.items)) return fail("bad list");
      d->shopTypes.push_back(t);
    } else if (kind == "link" && n >= 2) {
      int id = d->intern(w[1]);
      List changed;
      if (!d->parseList(w, raw, 2, changed)) return fail("bad list");
      if ((int)d->links.size() <= id) d->links.resize(id + 1);
      for (const Entry &e : changed)
        if (e.id != NONE) d->links[id].push_back({e.id, e.weight});
    } else if (kind == "boss" && (n == 3 || (n == 4 && isWeightWord(raw[3], from)))) {
      Boss b;
      if (n == 4) b.weight = from;
      b.id = d->intern(w[1]);
      if (w[2] != "-")
        for (char c : w[2]) {
          if (c < '1' || c > '0' + MAX_ANTE)
            return fail("expected: boss <key> <antes as digits, or ->");
          b.antes |= 1u << (c - '0');
        }
      d->bosses.push_back(b);
    } else {
      return fail("unknown or malformed record '" + kind + "'");
    }
  }
  if (!header) {
    lineNo = 1;
    return fail("expected 'gamedata 1'");
  }
  // The Soul and Black Hole come only from the rolls packs make for them, so
  // a weighted game's lists leave them out (unavailable items aren't listed).
  // The search still has to name them.
  d->intern("c_soul");
  d->intern("c_black_hole");
  return d;
}

std::string GameData::encodeWord(const std::string &word) {
  static const char HEX[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : word) {
    if (c == '%' || c <= ' ' || c == 0x7f) {
      out += '%';
      out += HEX[c >> 4];
      out += HEX[c & 15];
    } else {
      out += (char)c;
    }
  }
  return out;
}

std::string GameData::decodeWord(const std::string &word) {
  auto hex = [](char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
  };
  std::string out;
  for (size_t i = 0; i < word.size(); i++) {
    int hi = -1, lo = -1;
    if (word[i] == '%' && i + 2 < word.size() && (hi = hex(word[i + 1])) >= 0 &&
        (lo = hex(word[i + 2])) >= 0) {
      out += (char)(hi * 16 + lo);
      i += 2;
    } else {
      out += word[i];
    }
  }
  return out;
}

bool GameData::randomPack(int id) const {
  if (id == NONE) return false;
  for (int v : packs.value)
    if (v == id) return true;
  for (const Entry &e : packList)
    if (e.id == id) return true;
  return false;
}

std::string GameData::toText() const {
  std::string out = "gamedata 1\n";
  if (weighted) out += "weights 1\n";
  out += std::string("pad ") + (pad ? "1" : "0") + "\n";
  for (int a = 1; a <= MAX_ANTE; a++) {
    out += "tags " + std::to_string(a);
    writeList(out, *this, tags[a]);
    out += "\n";
  }
  for (const Voucher &v : vouchers) {
    out += "voucher " + enc(keyOf(v.id)) + (v.start ? " 1" : " 0") + (v.later ? " 1" : " 0");
    if (weighted) out += " " + weightWord(v.weight);
    for (int need : v.needs) out += " " + enc(keyOf(need));
    out += "\n";
  }
  if (firstPack != NONE) out += "firstpack " + enc(keyOf(firstPack)) + "\n";
  for (size_t i = 0; i < packs.from.size(); i++)
    out += "pack " + number(packs.from[i]) + " " + enc(keyOf(packs.value[i])) + "\n";
  out += "shop " + number(shopTotal) + " " + number(shopJoker) + " " +
         std::to_string(shopSlots) + "\n";
  out += "raritypolls " + std::to_string(rarityPolls) + "\n";
  for (size_t i = 0; i < rarities.from.size(); i++) {
    const RarityPool &p = rarityPools[rarities.value[i]];
    out += "rarity " + number(rarities.from[i]) + " " + enc(p.stream);
    writeList(out, *this, p.jokers);
    out += "\n";
  }
  for (size_t i = 1; i < editions.from.size(); i++)
    out += "edition " + number(editions.from[i]) + " " +
           (editions.value[i] == NONE ? "-" : enc(keyOf(editions.value[i]))) + "\n";
  out += std::string("soul ") + (soul ? "1" : "0") + "\n";
  out += std::string("blackhole ") + (blackHole ? "1" : "0") + "\n";
  out += "legendary " + enc(legendaryStream);
  writeList(out, *this, legendaries);
  out += "\n";
  for (const Boss &b : bosses) {
    std::string antes;
    for (int a = 1; a <= MAX_ANTE; a++)
      if (b.antes >> a & 1) antes += (char)('0' + a);
    out += "boss " + enc(keyOf(b.id)) + " " + (antes.empty() ? "-" : antes) +
           (weighted ? " " + weightWord(b.weight) : "") + "\n";
  }
  if (editionStream != "edisho") out += "editionstream " + enc(editionStream) + "\n";
  if (shopTarot || shopPlanet || shopPlaying || shopSpectral)
    out += "shoprates " + number(shopTarot) + " " + number(shopPlanet) + " " +
           number(shopPlaying) + " " + number(shopSpectral) + "\n";
  for (int i = 0; i < CARD_SETS; i++) {
    if (cards[i].empty()) continue;
    out += std::string("cards ") + CARD_SET_NAMES[i];
    writeList(out, *this, cards[i]);
    out += "\n";
  }
  for (const ModSoul &m : modSouls) {
    out += "modsoul " + enc(keyOf(m.id)) + " " + number(m.rate) + (m.repeat ? " 1" : " 0");
    for (int s = 0; s < CARD_SETS; s++)
      if (m.sets[s]) out += std::string(" ") + CARD_SET_NAMES[s];
    out += "\n";
  }
  if (weighted) {
    out += "streams " + enc(typeStream) + " " + enc(shopStream) + " " + enc(soulStream) + "\n";
    out += "vouchersperante " + std::to_string(vouchersPerAnte) + "\n";
    out += "packlist";
    writeList(out, *this, packList);
    out += "\n";
    for (const ShopType &t : shopTypes) {
      out += "shoptype " + enc(t.name) + " " + number(t.rate);
      writeList(out, *this, t.items);
      out += "\n";
    }
    for (size_t id = 0; id < links.size(); id++) {
      if (links[id].empty()) continue;
      List changed;
      for (const Link &l : links[id]) {
        Entry e;
        e.id = l.id, e.weight = l.weight;
        changed.push_back(e);
      }
      out += "link " + enc(keys[id]);
      writeList(out, *this, changed);
      out += "\n";
    }
  }
  return out;
}
