// filterprog.hpp — custom filters: a list of conditions, written as text,
// that a seed must all satisfy. This is what the in-game filter builder
// produces; the native side reads it once per search and then checks it for
// every seed.
//
// THE TEXT FORMAT
//   One condition per line (or separated by ';'). The first word says what
//   kind of condition it is, the rest are game keys and name=value options:
//
//     tag     tag_charm      ante=1 blind=small     (blind: small|big|any)
//     voucher v_telescope    ante=1
//     pack    p_arcana_mega  ante=1 within=2         (one of the first 2 shop packs)
//     joker   j_blueprint    ante=1 within=4 edition=e_negative
//     soul    ante=1 packs=1 size=5 legendary=j_perkeo
//     boss    bl_final_heart ante=8
//
//   One more line changes how jokers are predicted (for the whole filter):
//
//     assume enhanced
//
//   = assume the deck has one card of each enhancement (Steel, Stone, Lucky,
//   Gold, Glass) from the start. Five jokers only appear in shops once such
//   a card is in the deck (Steel Joker, Stone Joker, Lucky Cat, Golden
//   Ticket, Glass Joker); with this line they can be searched for, without
//   it a condition on them is an error (it could never match). It also
//   slightly changes other jokers of the same rarity (about 1 in 55 common
//   or uncommon shop jokers, mostly becoming one of those five), so a run
//   without those cards may differ there. Cavendish (needs Gros Michel to
//   die) can't be searched for.
//
//   `ante` may also be a range, e.g. ante=6-8: the condition then holds if
//   it holds in ANY of those antes (each with the same options), e.g.
//     joker j_burglar ante=6-8 within=100     (Burglar somewhere in antes 6-8)
//     voucher v_retcon ante=1-8               (Retcon by ante 8)
//
//   Every option has a default (ante=1, blind=any, within=2 for packs and
//   jokers, edition=any, packs=1, size=5, legendary=any). Lines starting
//   with '#' are comments. Keys are the game's own (G.P_CENTERS / G.P_TAGS /
//   G.P_BLINDS), so filters work in every game language.
//
// WHAT EACH CONDITION MEANS (the "reference" meaning, checked by
// tests/filterprog_test.cpp). Each condition is a fact about the seed on its
// own, as if it were the only thing you looked at, in a run started fresh:
//
//   tag      The tag offered for that ante's Small (blind=small) or Big
//            (blind=big) blind, or either.
//   voucher  The voucher in that ante's shop, assuming the vouchers of all
//            earlier antes were bought (upgraded vouchers only appear after
//            their base voucher).
//   pack     One of the first `within` booster packs offered in that ante's
//            shops (2 per shop; ante 1's first is always the Buffoon Pack).
//   joker    One of the first `within` shop cards of that ante (2 per shop,
//            plus 2 per reroll) is that joker, with that edition if given.
//            Assumes no vouchers bought. Legendary jokers never appear in
//            shops: use `soul`.
//   soul     Opening `packs` Arcana packs of `size` cards (5 = Jumbo/Mega,
//            3 = normal) in that ante, every one contains The Soul, and (if
//            given) every Soul gives that legendary joker.
//   boss     The boss blind of that ante, assuming the boss is never
//            rerolled (Director's Cut, Retcon and the Boss Tag reroll it,
//            which changes every later boss too). Showdown ("final")
//            bosses only appear on ante 8, the others from their minimum
//            ante on; a condition that can never hold is an error.
//
// HOW IT IS CHECKED FAST
//   Conditions are checked kind by kind, cheapest first (bosses, tags,
//   vouchers, packs, souls, shop jokers), stopping at the first failure. What a seed
//   offers in an ante is worked out only when a condition first needs it,
//   and then kept for the other conditions: e.g. every `joker` condition of
//   ante 1 reads the same shop scan (fastscan.hpp), which is extended only
//   as far as the longest `within` asks; "Burglar in antes 6-8" stops at
//   ante 6 if it is there. The kinds use separate random streams, so the
//   order never changes an answer; tests/filterprog_test.cpp compares the
//   fast check with the reference meaning above, condition by condition.
#pragma once

#include "instance.hpp"

#include <memory>
#include <string>
#include <vector>

struct Condition {
  enum Kind { TAG, VOUCHER, PACK, JOKER, SOUL, BOSS };
  Kind kind = TAG;
  Item item = Item::RETRY; // the tag/voucher/pack/joker/boss; SOUL: the legendary
                           // (RETRY = any)
  int ante = 1;            // first ante of the range
  int anteTo = 1;          // last ante of the range (= ante if no range)
  int within = 2;          // PACK, JOKER: how many packs / shop cards
  int blind = 0;           // TAG: 0 any, 1 small, 2 big
  Item edition = Item::RETRY; // JOKER: required edition (RETRY = any)
  int packs = 1;           // SOUL: how many Arcana packs, each with a Soul
  int size = 5;            // SOUL: cards per Arcana pack
};

class FilterProgram {
public:
  // Reads the text format above. On a mistake returns nullptr and sets
  // `error` to a message naming the line, e.g.
  //   "line 2: unknown joker 'j_blueprnt'".
  static std::unique_ptr<FilterProgram> parse(const std::string &text,
                                              std::string &error);

  // True if the seed satisfies every condition (fast path).
  bool matches(Seed &seed) const;

  // The reference meaning of one condition of this filter (slow, simple;
  // for tests and for the rare legendary check). Uses the filter's
  // `assume enhanced` setting.
  bool referenceCheck(const Condition &c, Seed &seed) const;

  bool assumesEnhanced() const { return assumeEnhanced; }

  const std::vector<Condition> &conditions() const { return all; }

  static const int MAX_ANTE = 8;

private:
  std::vector<Condition> all;
  bool assumeEnhanced = false;
  // The order conditions are checked in: by kind, cheapest first.
  std::vector<Condition> ordered;
  // What the fast check needs to know per ante (index 1..MAX_ANTE), so that
  // all conditions on one ante share one scan with the same options.
  bool tagAnte[MAX_ANTE + 1] = {};
  int maxTagAnte = 0, maxVoucherAnte = 0, maxBossAnte = 0;
  unsigned jokerRarities[MAX_ANTE + 1] = {}; // SHOP_* bits
  bool jokerEditions[MAX_ANTE + 1] = {};
  int soulSize[MAX_ANTE + 1] = {};

  bool referenceAt(const Condition &c, int ante, Seed &seed) const;
  friend struct SeedFacts;
};

// Converts a game key to an Item, for every key a filter may use: tags,
// vouchers, packs, jokers, editions, bosses. Returns Item::RETRY if unknown.
Item itemFromGameKey(const std::string &key);
// The reverse: the game key of a tag/voucher/pack/joker/edition/boss, or "".
std::string gameKeyOf(Item item);
