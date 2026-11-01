// fastscan.hpp — fast scans of shops and packs, the building blocks for
// joker filters: "is there a Turtle Bean in the next 150 ante 8 shop
// items?" (Bean, Burglar), "is there a Negative Blueprint in the first 4
// ante 1 shop items?", "does this Mega Arcana pack contain The Soul?" (Souls).
//
// WHY THEY CAN BE FASTER
//   Every random value the game draws comes from a named stream ("cdt8",
//   "rarity8sho", "Joker2sho8", "edisho8", ...). Each stream has its own
//   state, and drawing from one never changes another. Generating a shop
//   item draws from up to five streams:
//
//     cdt<ante>            joker, tarot or planet?        always needed
//     rarity<ante>sho      common, uncommon or rare?      needed for jokers
//     edisho<ante>         edition                        only if asked for
//     Joker1/2/3sho<ante>  which joker of that rarity     only the rarities asked for
//     etperpoll<ante>      stickers                       never (not needed)
//     Tarot/Planet...      which tarot/planet             never
//
//   A scan draws only the streams its question needs and leaves the others
//   alone. The answer is the same as from the full generator, and every
//   stream that *was* used ends up in exactly the state the full generator
//   would leave it in.
//
// THE ONE RULE
//   The skipped streams are never advanced, so nothing may rely on them
//   afterwards on the same Instance:
//    - don't call the full generators (nextShopItem(ante), ...) for an ante
//      after scanning it, and
//    - every scan of the same ante on the same Instance must use the same
//      options (rarities, editions). Continuing with the same options is
//      fine: Burglar's ante 8 scan continues Bean's correctly.
//   So: decide everything you want to know about one ante's shop up front
//   and do it with one set of options. ShopJokerScan::exact() reports false
//   if a scan's options differ from an earlier scan of the same ante.
//
//   tests/fastscan_test.cpp checks every result against the full generators.
#pragma once

#include "functions.hpp"

// Rarities a ShopJokerScan looks at, combined with |.
enum ShopRarity : unsigned {
  SHOP_COMMON = 1,
  SHOP_UNCOMMON = 2,
  SHOP_RARE = 4,
  SHOP_ALL_RARITIES = 7,
};
unsigned shopRarityBit(Item rarity); // Item::Common -> SHOP_COMMON, ...

class ShopJokerScan {
public:
  // Scans the shop of `ante` on `inst`, continuing from wherever its streams
  // currently are. `rarities`: which jokers it identifies (SHOP_* bits).
  // `editions`: whether it also works out each joker's edition.
  ShopJokerScan(Instance &inst, int ante, unsigned rarities,
                bool editions = false);

  struct Joker {
    Item joker;   // Item::RETRY: not a joker, or not one of the rarities
    Item edition; // the joker's edition if `editions`, else No_Edition
  };
  // Exactly what the next inst.nextShopItem(ante) would be, reduced to the
  // fields above.
  Joker next();

  // False if an earlier scan of this ante on this Instance used different
  // options, i.e. this scan's results cannot be trusted (THE ONE RULE).
  bool exact() const { return exact_; }

private:
  // One rarity's joker choice: Instance::randchoice(key, pool).
  struct Pool {
    const Item *items = nullptr;
    int size = 0;
    std::string key;
    double *node = nullptr;
    double *resample[10] = {}; // "<key>_resample2" .. 9, looked up on use
  };
  Instance &inst;
  unsigned rarities;
  bool editions;
  bool exact_ = true;
  double totalRate, jokerRate;
  double negative, polychrome, holographic, foil; // edition thresholds
  Pool pools[3]; // Common, Uncommon, Rare
  // Stream states inside inst.cache (see Instance::node_ref).
  double *cardType, *rarityPoll, *edition = nullptr;

  Item pick(Pool &pool);
};

// The first match for `joker` among the next `items` shop items of `ante`
// (0 = the very next one), or -1. `edition`: also require this edition
// (Item::RETRY = any edition). A shorthand for a ShopJokerScan with just
// that joker's rarity, so THE ONE RULE applies.
int findShopJoker(Instance &inst, int ante, Item joker, int items,
                  Item edition = Item::RETRY);

// The rarity of `joker` (Common, Uncommon, Rare, Legendary) in the game
// version `version` (InstParams::version; default 1.0.1c), or Item::RETRY if
// it is not a joker. Version matters: a few jokers changed rarity in 1.0.1
// (e.g. Vagabond was uncommon in 1.0.0, rare since).
Item jokerRarity(Item joker, long version = 10103);

// Exactly whether inst.nextArcanaPack(size, ante) contains The Soul, with the
// same effect on the soul stream and on inst's locks. Skips the tarot choice
// streams (see THE ONE RULE above).
bool arcanaPackHasSoul(Instance &inst, int size, int ante);
