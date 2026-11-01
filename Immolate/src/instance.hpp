#include "items.hpp"
#include "seed.hpp"
#include "util.hpp"
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#pragma once

// The per-seed RNG state: one value per random "stream" the game uses
// ("Tag1", "cdt1", "Voucher1_resample2", ...), keyed by the stream's name.
//
// Performance: this used to be a std::map<std::string, double>, which costs
// a tree walk plus a memory allocation per new stream, on every seed. A seed
// only ever touches a few dozen streams, so a small fixed table searched
// linearly is much faster and never allocates. Anything that doesn't fit
// (more than CAPACITY streams, or a name longer than MAX_KEY characters)
// goes to `overflow`, a std::map exactly as before, so behaviour can never
// change, only speed.
struct NodeCache {
  static const int CAPACITY = 64;
  static const int MAX_KEY = 47;
  struct Entry {
    double value;
    uint8_t len;
    char key[MAX_KEY];
  };
  Entry entries[CAPACITY];
  int count = 0;
  std::map<std::string, double> overflow;

  NodeCache() {}
  // Copy only the entries in use (normally none: the cache is empty when
  // brainstormFilter copies the Instance at the start of each seed).
  NodeCache(const NodeCache &o) : count(o.count), overflow(o.overflow) {
    std::memcpy(entries, o.entries, sizeof(Entry) * count);
  }
  NodeCache &operator=(const NodeCache &o) {
    count = o.count;
    overflow = o.overflow;
    std::memcpy(entries, o.entries, sizeof(Entry) * count);
    return *this;
  }

  void clear() {
    count = 0;
    if (!overflow.empty()) overflow.clear();
  }

  // Returns the stored value for `id`, or nullptr if this stream is new.
  double *find(const std::string &id) {
    size_t n = id.size();
    if (n <= (size_t)MAX_KEY) {
      for (int i = 0; i < count; i++) {
        Entry &e = entries[i];
        if (e.len == n && std::memcmp(e.key, id.data(), n) == 0)
          return &e.value;
      }
    }
    if (overflow.empty()) return nullptr;
    auto it = overflow.find(id);
    return it == overflow.end() ? nullptr : &it->second;
  }

  // Adds a new stream (caller has checked find() returned nullptr).
  double *insert(const std::string &id, double value) {
    size_t n = id.size();
    if (count < CAPACITY && n <= (size_t)MAX_KEY) {
      Entry &e = entries[count++];
      e.value = value;
      e.len = (uint8_t)n;
      std::memcpy(e.key, id.data(), n);
      return &e.value;
    }
    return &(overflow[id] = value);
  }
};

struct Cache {
  NodeCache nodes;
  bool generatedFirstPack = false;
  // Options of the first ShopJokerScan of each ante (src/fastscan.hpp), 0 if
  // none yet. Lets a later scan of the same ante detect that it would need a
  // stream the first one skipped.
  uint8_t shopScan[9] = {0};
};

struct InstParams {
  Item deck;
  Item stake;
  bool showman;
  int sixesFactor;
  long version;
  bool vouchers[32] = {false};
  InstParams() {
    deck = Item::Red_Deck;
    stake = Item::White_Stake;
    showman = false;
    sixesFactor = 1;
    version = 10103; // 1.0.1c
  }
  InstParams(Item d, Item s, bool show, long v) {
    deck = d;
    stake = s;
    showman = show;
    sixesFactor = 1;
    version = v;
  }
};

// Index Instance::randweightedchoice picks: walks the cumulative weights
// until they reach random() * total. Larger random() never picks an earlier
// index, so it is monotone and can use luaRandomDecide.
template <std::size_t N>
int weightedIndex(double seed, const std::array<WeightedItem, N> &items) {
  return luaRandomDecide(seed, [&](double x) {
    double poll = x * items[0].weight;
    int idx = 1;
    double weight = 0;
    while (weight < poll) {
      weight += items[idx].weight;
      idx++;
    }
    return idx - 1;
  });
}

struct Instance {
  bool locked[(int)Item::ITEMS_END] = {false};
  // How many times each boss blind (in BOSSES order) has been chosen so far
  // this run; the game's G.GAME.bosses_used. See nextBoss().
  uint8_t bossesUsed[28] = {0};
  Seed &seed;
  double hashedSeed;
  Cache cache;
  InstParams params;
  LuaRandom rng;
  Instance(Seed &s) : seed(s) {
    hashedSeed = s.pseudohash(0);
    params = InstParams();
    rng = LuaRandom(0);
  };
  void reset(Seed &s) { // This is slow, use next() unless necessary
    seed = s;
    hashedSeed = s.pseudohash(0);
    params = InstParams();
    cache.nodes.clear();
    cache.generatedFirstPack = false;
    std::memset(cache.shopScan, 0, sizeof cache.shopScan);
  };
  void next() {
    seed.next();
    hashedSeed = seed.pseudohash(0);
    params = InstParams();
    cache.nodes.clear();
    cache.generatedFirstPack = false;
    std::memset(cache.shopScan, 0, sizeof cache.shopScan);
  }
  // Advances the stream `ID` one step and returns the value the game would
  // seed math.random with (Balatro's pseudoseed()).
  double get_node(const std::string &ID) { return step_node(node_ref(ID)); }
  // The two halves of get_node(), for hot loops (src/fastscan.hpp) that use
  // the same stream many times: look the stream up once with node_ref(),
  // then advance it with step_node() without searching the cache again.
  // The pointer stays valid until the cache is cleared (entries never move;
  // std::map nodes don't either).
  double *node_ref(const std::string &ID) {
    double *node = cache.nodes.find(ID);
    if (!node) {
      node = cache.nodes.insert(
          ID, pseudohash_from(ID, seed.pseudohash(ID.length())));
    }
    return node;
  }
  double step_node(double *node) {
    *node = advance_node(*node);
    return (*node + hashedSeed) / 2;
  }
  // One step of a stream's state, without storing it anywhere.
  static double advance_node(double node) {
    return round13Fast(fractFast(node * 1.72431234 + 2.134453429141));
  }
  double random(const std::string &ID) {
    rng = LuaRandom(get_node(ID));
    return rng.random();
  }
  // randint / randchoice / randweightedchoice only need *which* value or
  // item comes out, so they use luaRandomDecide (util.hpp): same result as
  // running LuaRandom in full, usually without doing so.
  int randint(const std::string &ID, int min, int max) {
    return luaRandint(get_node(ID), min, max);
  }
  template <std::size_t N>
  Item randchoice(const std::string &ID, const std::array<Item, N> &items) {
    Item item = items[luaRandint(get_node(ID), 0, items.size() - 1)];
    if ((params.showman == false && isLocked(item)) || item == Item::RETRY) {
      int resample = 2;
      while (true) {
        Item item = items[luaRandint(
            get_node(ID + "_resample" + anteToString(resample)), 0,
            items.size() - 1)];
        resample++;
        if ((item != Item::RETRY && !isLocked(item)) || resample > 1000)
          return item;
      }
    }
    return item;
  }
  template <std::size_t N>
  Item randweightedchoice(const std::string &ID,
                          const std::array<WeightedItem, N> &items) {
    return items[weightedIndex(get_node(ID), items)].item;
  }

  // Functions defined in functions.hpp
  void lock(Item item);
  void unlock(Item item);
  bool isLocked(Item item);
  void initLocks(int ante, bool freshProfile, bool freshRun);
  void initUnlocks(int ante, bool freshProfile);
  Item nextTarot(std::string source, int ante, bool soulable);
  Item nextPlanet(std::string source, int ante, bool soulable);
  Item nextSpectral(std::string source, int ante, bool soulable);
  JokerData nextJoker(std::string source, int ante, bool hasStickers);
  ShopInstance getShopInstance();
  ShopItem nextShopItem(int ante);
  Item nextPack(int ante);
  std::vector<Item> nextArcanaPack(int size, int ante);
  std::vector<Item> nextCelestialPack(int size, int ante);
  std::vector<Item> nextSpectralPack(int size, int ante);
  std::vector<JokerData> nextBuffoonPack(int size, int ante);
  std::vector<Card> nextStandardPack(int size, int ante);
  Card nextStandardCard(int ante);
  bool isVoucherActive(Item voucher);
  void activateVoucher(Item voucher);
  Item nextVoucher(int ante);
  void setDeck(Item deck);
  void setStake(Item stake);
  Item nextTag(int ante);
  Item nextBoss(int ante);
};