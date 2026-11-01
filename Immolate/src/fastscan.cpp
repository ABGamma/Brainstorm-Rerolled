// fastscan.cpp — see fastscan.hpp. Every step here mirrors a specific piece
// of functions.hpp / instance.hpp; the comments say which.

#include "fastscan.hpp"

namespace {

template <std::size_t N>
bool contains(const std::array<Item, N> &pool, Item item) {
  for (Item i : pool)
    if (i == item) return true;
  return false;
}


} // namespace

Item jokerRarity(Item joker, long version) {
  // Same pools as Instance::nextJoker picks from for this version.
  if (version > 10099) {
    if (contains(COMMON_JOKERS, joker)) return Item::Common;
    if (contains(UNCOMMON_JOKERS, joker)) return Item::Uncommon;
    if (contains(RARE_JOKERS, joker)) return Item::Rare;
  } else {
    if (contains(COMMON_JOKERS_100, joker)) return Item::Common;
    if (contains(UNCOMMON_JOKERS_100, joker)) return Item::Uncommon;
    if (contains(RARE_JOKERS_100, joker)) return Item::Rare;
  }
  if (contains(LEGENDARY_JOKERS, joker)) return Item::Legendary;
  return Item::RETRY;
}

unsigned shopRarityBit(Item rarity) {
  return rarity == Item::Common     ? SHOP_COMMON
         : rarity == Item::Uncommon ? SHOP_UNCOMMON
         : rarity == Item::Rare     ? SHOP_RARE
                                    : 0; // Legendary: never in shops
}

ShopJokerScan::ShopJokerScan(Instance &inst_, int ante, unsigned rarities_,
                             bool editions_)
    : inst(inst_), rarities(rarities_ & SHOP_ALL_RARITIES),
      editions(editions_) {
  // THE ONE RULE: remember the options of the first scan of this ante.
  uint8_t mode = (uint8_t)(0x80 | rarities | (editions ? 8 : 0));
  if (ante >= 0 && ante < 9) {
    uint8_t &first = inst.cache.shopScan[ante];
    if (first == 0) first = mode;
    else exact_ = first == mode;
  }

  // Instance::nextShopItem: the shop rates don't change while scanning
  // (nothing in a scan activates vouchers).
  ShopInstance shop = inst.getShopInstance();
  totalRate = shop.getTotalRate();
  jokerRate = shop.jokerRate;

  // Instance::nextJoker: edition thresholds, same expressions.
  int editionRate = 1;
  if (inst.isVoucherActive(Item::Glow_Up)) editionRate = 4;
  else if (inst.isVoucherActive(Item::Hone)) editionRate = 2;
  negative = 0.997;
  polychrome = 1 - 0.006 * editionRate;
  holographic = 1 - 0.02 * editionRate;
  foil = 1 - 0.04 * editionRate;

  // Instance::nextJoker: stream names and pools (version > 10099 = 1.0.1+).
  std::string anteStr = anteToString(ante);
  bool v101 = inst.params.version > 10099;
  const std::string *types[3] = {&RandomType::Joker_Common,
                                 &RandomType::Joker_Uncommon,
                                 &RandomType::Joker_Rare};
  for (int r = 0; r < 3; r++) {
    if (!(rarities & (1u << r))) continue;
    Pool &p = pools[r];
    if (r == 0) {
      p.items = v101 ? COMMON_JOKERS.data() : COMMON_JOKERS_100.data();
      p.size = v101 ? (int)COMMON_JOKERS.size() : (int)COMMON_JOKERS_100.size();
    } else if (r == 1) {
      p.items = v101 ? UNCOMMON_JOKERS.data() : UNCOMMON_JOKERS_100.data();
      p.size =
          v101 ? (int)UNCOMMON_JOKERS.size() : (int)UNCOMMON_JOKERS_100.size();
    } else {
      p.items = v101 ? RARE_JOKERS.data() : RARE_JOKERS_100.data();
      p.size = v101 ? (int)RARE_JOKERS.size() : (int)RARE_JOKERS_100.size();
    }
    p.key = *types[r] + ItemSource::Shop + anteStr;
    p.node = inst.node_ref(p.key);
  }

  cardType = inst.node_ref(RandomType::Card_Type + anteStr);
  rarityPoll =
      inst.node_ref(RandomType::Joker_Rarity + anteStr + ItemSource::Shop);
  if (editions)
    edition = inst.node_ref(RandomType::Joker_Edition + ItemSource::Shop + anteStr);
}

// Instance::randchoice(pool.key, pool), including its re-rolls.
Item ShopJokerScan::pick(Pool &pool) {
  Item item = pool.items[luaRandint(inst.step_node(pool.node), 0, pool.size - 1)];
  if ((inst.params.showman == false && inst.isLocked(item)) ||
      item == Item::RETRY) {
    int r = 2;
    while (true) {
      double *node;
      if (r < 10) {
        if (!pool.resample[r])
          pool.resample[r] =
              inst.node_ref(pool.key + "_resample" + anteToString(r));
        node = pool.resample[r];
      } else {
        node = inst.node_ref(pool.key + "_resample" + anteToString(r));
      }
      Item again =
          pool.items[luaRandint(inst.step_node(node), 0, pool.size - 1)];
      r++;
      if ((again != Item::RETRY && !inst.isLocked(again)) || r > 1000)
        return again;
    }
  }
  return item;
}

ShopJokerScan::Joker ShopJokerScan::next() {
  Joker none = {Item::RETRY, Item::No_Edition};
  // All the tests below are monotone in random(), so luaRandomDecide gives
  // exactly the full computation's answer (see util.hpp).

  // Instance::nextShopItem: card type.
  bool joker = luaRandomDecide(inst.step_node(cardType), [&](double x) {
    return x * totalRate < jokerRate;
  });
  if (!joker) return none; // tarot/planet/...: its streams are skipped

  // Instance::nextJoker, in its order: rarity (shop jokers always poll for
  // it), edition (every joker, whatever its rarity, so the edition stream
  // stays in step), then the choice within the rarity. The sticker poll
  // that follows is skipped.
  int r = luaRandomDecide(inst.step_node(rarityPoll), [](double x) {
    return x > 0.95 ? 2 : x > 0.7 ? 1 : 0; // Rare, Uncommon, Common
  });
  Item ed = Item::No_Edition;
  if (editions) {
    int e = luaRandomDecide(inst.step_node(edition), [&](double x) {
      return x > negative      ? 4
             : x > polychrome  ? 3
             : x > holographic ? 2
             : x > foil        ? 1
                               : 0;
    });
    const Item byIndex[5] = {Item::No_Edition, Item::Foil, Item::Holographic,
                             Item::Polychrome, Item::Negative};
    ed = byIndex[e];
  }
  if (!(rarities & (1u << r))) return none; // that rarity's pool is skipped
  return {pick(pools[r]), ed};
}

int findShopJoker(Instance &inst, int ante, Item joker, int items,
                  Item edition) {
  bool wantEdition = edition != Item::RETRY;
  ShopJokerScan scan(inst, ante,
                     shopRarityBit(jokerRarity(joker, inst.params.version)),
                     wantEdition);
  for (int i = 0; i < items; i++) {
    ShopJokerScan::Joker j = scan.next();
    if (j.joker == joker && (!wantEdition || j.edition == edition)) return i;
  }
  return -1;
}

bool arcanaPackHasSoul(Instance &inst, int size, int ante) {
  // The shortcut below covers the normal case. Anything that would make the
  // tarot choices themselves matter goes through the full generator:
  //  - Omen Globe turns some cards into spectrals (own soul rolls);
  //  - Showman changes the lock rules;
  //  - a locked tarot would end up *unlocked* by nextArcanaPack.
  bool simple = !inst.isVoucherActive(Item::Omen_Globe) && !inst.params.showman;
  for (Item t : TAROTS)
    if (inst.isLocked(t)) simple = false;
  if (!simple) {
    for (Item it : inst.nextArcanaPack(size, ante))
      if (it == Item::The_Soul) return true;
    return false;
  }

  // Instance::nextArcanaPack + nextTarot(soulable = true): each card first
  // rolls the soul stream, unless The Soul is locked. A Soul is locked as
  // soon as it is generated, so at most one per pack, and the cards after
  // it don't roll the soul stream. The tarot choice stream is skipped.
  if (inst.isLocked(Item::The_Soul)) return false; // never rolled at all
  double *soul = inst.node_ref(RandomType::Soul + RandomType::Tarot +
                               anteToString(ante));
  for (int i = 0; i < size; i++) {
    if (luaRandomDecide(inst.step_node(soul),
                        [](double x) { return x > 0.997; })) {
      // nextArcanaPack locks it, then unlocks everything it generated at the
      // end: The Soul ends up unlocked, as it started.
      return true;
    }
  }
  return false;
}
