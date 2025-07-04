#include "items.hpp"
#include "seed.hpp"
#include "util.hpp"
#include <map>
#include <string>
#pragma once

struct Cache {
  std::map<std::string, double> nodes;
  bool generatedFirstPack = false;
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

struct Instance {
  bool locked[(int)Item::ITEMS_END] = {false};
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
    cache.nodes
      .clear(); // Somehow `clear` is faster than swapping with empty map
    cache.generatedFirstPack = false;
  };
  void next() {
    seed.next();
    hashedSeed = seed.pseudohash(0);
    params = InstParams();
    cache.nodes.clear();
    cache.generatedFirstPack = false;
  }
  double get_node(std::string ID) {
    if (cache.nodes.count(ID) == 0) {
      cache.nodes[ID] = pseudohash_from(ID, seed.pseudohash(ID.length()));
    }
    cache.nodes[ID] =
        round13(fract(cache.nodes[ID] * 1.72431234 + 2.134453429141));
    return (cache.nodes[ID] + hashedSeed) / 2;
  }
  double random(std::string ID) {
    rng = LuaRandom(get_node(ID));
    return rng.random();
  }
  int randint(std::string ID, int min, int max) {
    rng = LuaRandom(get_node(ID));
    return rng.randint(min, max);
  }
  template <std::size_t N>
  Item randchoice(std::string ID, const std::array<Item, N> &items) {
    rng = LuaRandom(get_node(ID));
    Item item = items[rng.randint(0, items.size() - 1)];
    if ((params.showman == false && isLocked(item)) || item == Item::RETRY) {
      int resample = 2;
      while (true) {
        rng = LuaRandom(get_node(ID + "_resample" + anteToString(resample)));
        Item item = items[rng.randint(0, items.size() - 1)];
        resample++;
        if ((item != Item::RETRY && !isLocked(item)) || resample > 1000)
          return item;
      }
    }
    return item;
  }
  template <std::size_t N>
  Item randweightedchoice(std::string ID,
                          const std::array<WeightedItem, N> &items) {
    rng = LuaRandom(get_node(ID));
    double poll = rng.random() * items[0].weight;
    int idx = 1;
    double weight = 0;
    while (weight < poll) {
      weight += items[idx].weight;
      idx++;
    }
    return items[idx - 1].item;
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