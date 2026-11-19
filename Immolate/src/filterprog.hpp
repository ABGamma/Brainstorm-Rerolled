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
//     joker   j_blueprint    ante=1 from=buffoon packs=1 size=2
//     soul    ante=1 packs=1 size=5 legendary=j_perkeo
//     card    c_temperance   ante=1 from=arcana packs=1 size=5
//     card    c_black_hole   ante=2 from=celestial
//     card    c_hermit       ante=1 from=shop within=4
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
//     assume showman
//
//   = assume a Showman is held for the whole run, so the game's "no
//   duplicates" rule is off everywhere a filter looks:
//     - shop jokers: a joker can appear again in the same shop (without it,
//       a joker on screen can't be drawn again until the shop is rerolled);
//     - The Soul: a legendary can come again (without it, asking for the
//       same legendary from two Souls is an error), and a pack can hold
//       more than one Soul.
//   Tags, vouchers, pack kinds and bosses don't depend on it. A real run
//   without a Showman (most of them) may then differ from the search.
//
//   A condition that can never hold in a fresh run is an error, with the
//   reason, instead of a search that never ends: a tag on an ante before it
//   unlocks, an upgraded voucher on ante 1, a pack that can't be among the
//   first packs asked about, a boss on an ante where the game never picks it.
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
//            Assumes no vouchers bought and nothing bought from the shop: a
//            joker can't appear twice in one shop (the game re-rolls the
//            second), but can come back after a reroll. Legendary jokers
//            never appear in shops: use `soul`.
//            `from=buffoon`: in one of the first `packs` Buffoon packs
//            opened in that ante instead, of `size` cards (2 = normal, 4 =
//            Jumbo/Mega; default 2), with no shop joker or joker of yours
//            in the way (they can't come again). A joker can't appear twice
//            in one pack. All Buffoon conditions of one ante open the same
//            packs, so they must use the same size. Ante 1's first shop
//            pack is always a normal Buffoon pack.
//   soul     Opening `packs` Arcana packs of `size` cards (5 = Jumbo/Mega,
//            3 = normal) in that ante, every one contains The Soul, and (if
//            given) every Soul gives that legendary joker, with that
//            `edition` (if given). `from=spectral`: Spectral packs instead
//            (size 4 = Jumbo/Mega, 2 = normal). A Spectral card also rolls
//            for a Black Hole on the same random stream, and a Black Hole
//            wins over a Soul, so the two kinds of pack differ. All soul
//            conditions of one ante must open the same kind and size of
//            pack. Where the packs come from doesn't matter (shop, or a
//            tag: Charm gives a Mega Arcana, Ethereal, from ante 2 on, a
//            normal Spectral pack); a `pack` condition can ask for them. Legendaries come
//            in one fixed order per seed, whatever the ante: the first Soul
//            used gives the first, the second the next, and so on. Soul
//            conditions on single antes are read as one run in ante order
//            ("soul ante=1" then "soul ante=2 legendary=j_perkeo": Perkeo is
//            the SECOND legendary). Several soul conditions on the same
//            ante take its Souls one after another, in the order written:
//            "soul ante=1 legendary=j_perkeo" then "soul ante=1
//            legendary=j_triboulet" is Perkeo from the first pack and
//            Triboulet from the second (`packs=2` is the same as writing a
//            condition twice). A soul condition on an ante range
//            ("ante=1-3") is read on its own, as the run's first Souls,
//            after those of earlier conditions on the same range.
//   card     A Tarot, Planet or Spectral card (or The Soul / Black Hole):
//            `from=arcana|celestial|spectral` (default: the pack of its
//            kind) in one of the first `packs` packs of that kind opened in
//            that ante, of `size` cards (Arcana and Celestial 5 or 3,
//            default 5; Spectral 4 or 2, default 4); or `from=shop` among
//            the first `within` shop cards (as for jokers). Packs are
//            counted like soul conditions count them: with "soul ante=1"
//            and "card c_temperance ante=1", Temperance is in the same pack
//            as The Soul. Every card of a pack first rolls for The Soul /
//            Black Hole (Arcana: The Soul; Celestial: Black Hole; Spectral:
//            both), then, if that missed, draws from its list; no card twice
//            in one pack or one shop. Assumes no Omen Globe or Telescope.
//            A mod's Soul-like consumable (Steamodded: Cryptid's Gateway,
//            White Hole, ...) is rolled for before The Soul / Black Hole,
//            which win over it; it can be searched for in the packs that
//            roll for it.
//            In games with Steamodded's object weights (Pokermon), a pack
//            card that isn't The Soul / Black Hole is picked by weight
//            (poll_object) on the pack's own stream.
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

#include "gamedata.hpp"
#include "instance.hpp"

#include <memory>
#include <string>
#include <vector>

struct Condition {
  enum Kind { TAG, VOUCHER, PACK, JOKER, SOUL, BOSS, CARD };
  // CARD: where to look (`from`).
  enum From { SHOP, ARCANA, CELESTIAL, SPECTRAL };
  Kind kind = TAG;
  // The tag/voucher/pack/joker/boss; for SOUL the legendary. `id` is its
  // number in the filter's GameData (NONE = any legendary). `item` is the
  // same thing as an Item when the unmodded game has it (RETRY otherwise,
  // and for "any"); only the reference check and the tests use it.
  int id = GameData::NONE;
  Item item = Item::RETRY;
  int ante = 1;            // first ante of the range
  int anteTo = 1;          // last ante of the range (= ante if no range)
  int within = 2;          // PACK, JOKER: how many packs / shop cards
  int blind = 0;           // TAG: 0 any, 1 small, 2 big
  // JOKER: the required edition. anyEdition: none required. Otherwise
  // editionId (NONE = "e_base", no edition); `edition` is the Item form.
  bool anyEdition = true;
  int editionId = GameData::NONE;
  Item edition = Item::RETRY;
  int packs = 1;           // SOUL: how many packs, each with a Soul;
                           // JOKER from=buffoon: how many Buffoon packs
  // SOUL: Souls of the same ante (or the same range) that earlier soul
  // conditions asked for; this one's packs come after theirs. Set by parse.
  int first = 0;
  int size = 5;            // SOUL, JOKER from=buffoon: cards per pack
  bool buffoon = false;    // JOKER: in Buffoon packs instead of the shop
  int from = SHOP;         // CARD: the shop, or a kind of pack (From)
  int set = 0;             // CARD: 0 Tarot, 1 Planet, 2 Spectral (GameData::cards)
  bool spectral = false;   // SOUL: Spectral packs instead of Arcana packs
};

class FilterProgram {
public:
  // Reads the text format above, for the game described by `data`. On a
  // mistake returns nullptr and sets `error` to a message naming the line,
  // e.g. "line 2: unknown joker 'j_blueprnt'".
  static std::unique_ptr<FilterProgram>
  parse(const std::string &text, std::string &error,
        std::shared_ptr<const GameData> data = GameData::vanilla());

  // True if the seed satisfies every condition (fast path).
  bool matches(Seed &seed) const;
  // The same for an Instance that is on the seed with nothing drawn yet
  // (fresh, or just after Instance::next()). Saves building one per seed.
  bool matches(Instance &inst) const;

  // The reference meaning of one condition of this filter (slow, simple,
  // from Immolate's full generators; for tests). Only for filters on
  // GameData::vanilla().
  bool referenceCheck(const Condition &c, Seed &seed) const;

  bool assumesEnhanced() const { return assumeEnhanced; }
  bool assumesShowman() const { return assumeShowman; }
  const std::vector<Condition> &conditions() const { return all; }
  const GameData &data() const { return *gd; }
  const std::shared_ptr<const GameData> &dataPtr() const { return gd; }

  // What a run on `seed` starts with: ante 1's Small and Big Blind tags,
  // its shop voucher and its boss, as numbers in `data` (NONE if unknown).
  // The mod compares these with the real run after starting it.
  struct Start {
    int small = GameData::NONE, big = GameData::NONE;
    int voucher = GameData::NONE, boss = GameData::NONE;
  };
  static Start predictStart(std::shared_ptr<const GameData> data, Seed &seed);

  // Everything predicted for the first `antes` antes of a run on `seed`, as
  // text, one line per ante:
  //   ante=1 small=<tag> big=<tag> voucher=<key> boss=<key>
  //     shop=<card>,<card>,... packs=<pack>,... legend=<joker>,<joker>
  //     arcana=<card>,...|<card>,... celestial=... spectralpacks=...
  //     buffoon=<joker>,<joker>,<joker>,<joker>
  // (on one line). shop: consumables too. arcana,
  // celestial, spectralpacks: the cards of the first two packs of that kind
  // opened in that ante (5, 5 and 4 cards), "|" between the packs. buffoon:
  // the jokers of the first two normal Buffoon packs opened in that ante
  // (with editions, as for shop). shop: the first `cards` shop cards (rerolls included); a
  // joker as key or key/edition, "-" for a card that isn't predicted (in an
  // unweighted game, anything but a joker). packs: the first `packs` shop
  // packs. legend: what the first two Souls opened in that ante would give
  // if no Soul was used before, as key or key/edition. For tests, which
  // compare it with what the real game generated.
  static std::string describe(std::shared_ptr<const GameData> data, Seed &seed,
                              int antes, int cards, int packs);

  static const int MAX_ANTE = GameData::MAX_ANTE;

  // Stream names: "<name>" and its re-rolls "<name>_resample2", ...
  struct StreamKeys {
    std::string base;
    std::string re[8]; // _resample2 .. _resample9
    void set(const std::string &name);
  };

  // A weighted list as the fast check reads it.
  struct WPool {
    std::vector<int> id;     // NONE: a slot without an item (rolled again)
    std::vector<double> w;   // weights while nothing else is on offer
    std::vector<double> cum; // running totals of `w`, as the game adds them
    // For lists whose weights change with what is on offer (shop, Soul):
    std::vector<int> indexOf;   // item id -> position in the list, or -1
    std::vector<char> affected; // item id -> having it on offer changes `w`
  };

private:
  std::shared_ptr<const GameData> gd;
  std::vector<Condition> all;
  bool assumeEnhanced = false;
  bool assumeShowman = false;

  // The order conditions are checked in: by kind, cheapest first.
  std::vector<Condition> ordered;

  // What the fast check needs to know per ante (index 1..MAX_ANTE), so that
  // all conditions on one ante share one scan with the same options.
  bool tagAnte[MAX_ANTE + 1] = {};
  int maxTagAnte = 0, maxVoucherAnte = 0, maxBossAnte = 0;
  std::vector<char> jokerRarities[MAX_ANTE + 1]; // per rarity pool: needed?
  bool jokerEditions[MAX_ANTE + 1] = {};
  int soulSize[MAX_ANTE + 1] = {};
  // Jokers in Buffoon packs: cards per pack in that ante (0: no condition
  // asks), packs to open, and like the shop which rarities and editions.
  int bufSize[MAX_ANTE + 1] = {}, bufPacks[MAX_ANTE + 1] = {};
  // Consumable cards (CARD): per kind of pack (Condition::From), the cards
  // per pack in that ante (0: no condition asks); per set, whether the shop
  // is asked about.
  int cardPackSize[MAX_ANTE + 1][4] = {};
  bool cardShop[MAX_ANTE + 1][GameData::CARD_SETS] = {};
  std::vector<char> bufRarities[MAX_ANTE + 1];
  bool bufEditions[MAX_ANTE + 1] = {};
  char soulKind[MAX_ANTE + 1] = {}; // 0: no soul condition, 1 Arcana, 2 Spectral
  // Soul conditions on single antes, read as one run (see "soul" above):
  // packs opened in each ante (all its soul conditions together), and Souls
  // used before it.
  int soulPacks[MAX_ANTE + 1] = {};
  int soulBase[MAX_ANTE + 2] = {};

  // The lists of `gd` as the game sees them for this filter: ids, with
  // GameData::NONE in every slot that is unavailable.
  std::vector<int> tagPool[MAX_ANTE + 1];
  std::vector<std::vector<int>> jokerPool; // per rarity pool
  std::vector<int> legendaryPool;
  std::vector<int> voucherIndex;               // id -> index in gd->vouchers
  std::vector<std::vector<int>> voucherUnlocks; // index -> vouchers needing it

  StreamKeys tagKeys[MAX_ANTE + 1], voucherKeys[MAX_ANTE + 1];
  std::string packKey[MAX_ANTE + 1], cdtKey[MAX_ANTE + 1],
      rarityKey[MAX_ANTE + 1], editionKey[MAX_ANTE + 1], soulKey[MAX_ANTE + 1], soulEditionKey[MAX_ANTE + 1],
      soulSpectralKey[MAX_ANTE + 1], bufRarityKey[MAX_ANTE + 1],
      bufEditionKey[MAX_ANTE + 1];
  std::vector<StreamKeys> jokerKeys[MAX_ANTE + 1]; // per rarity pool
  std::vector<StreamKeys> bufJokerKeys[MAX_ANTE + 1]; // per rarity pool
  // Consumables: the lists (as tagPool), The Soul's and Black Hole's ids,
  // the streams of the packs' card lists, of their Soul / Black Hole rolls
  // (index: Condition::From), and of the shop's card lists (index: set).
  std::vector<int> cardPool[GameData::CARD_SETS];
  int soulCard = GameData::NONE, holeCard = GameData::NONE;
  StreamKeys cardPackKeys[MAX_ANTE + 1][4], cardShopKeys[MAX_ANTE + 1][GameData::CARD_SETS];
  std::string cardRollKey[MAX_ANTE + 1][4];
  // Mods' Soul-like consumables (GameData::modSouls) per kind of pack, and
  // the stream of their roll ("soul_smods_" + set + ante).
  std::vector<GameData::ModSoul> modSouls[4];
  std::string cardModRollKey[MAX_ANTE + 1][4];
  // Weighted games: the stream a pack card is picked from (poll_object with
  // the pack's key_append: "std_smods_pollar1" + ante, ...).
  std::string cardPackStream[MAX_ANTE + 1][4];
  // Weighted games: the stream a Buffoon pack joker is picked from
  // ("std_smods_pollbuf" + ante).
  std::string bufStream[MAX_ANTE + 1];
  StreamKeys legendaryKeys;

  // --- Weighted games (gamedata.hpp, "WEIGHTED GAMES") ---
  WPool wTags[MAX_ANTE + 1], wPacks, wLegendary;
  std::vector<WPool> wRarity; // per rarity pool
  std::vector<WPool> wTypes;  // per GameData::shopTypes entry
  std::vector<WPool> wCards;  // per GameData::cards set (pack cards)
  std::vector<double> wTypeCum; // running totals of the shop type rates
  std::string typeKey[MAX_ANTE + 1], shopKey[MAX_ANTE + 1],
      soulDrawKey[MAX_ANTE + 1];
  WPool makePool(const GameData::List &list, bool changing) const;

  void resolve();                           // fills the lists and names above
  std::string impossible(const Condition &c) const; // "" or the reason
  bool referenceAt(const Condition &c, int ante, Seed &seed) const;
  friend struct SeedFacts;
  friend class Prefilter;
};

// Converts a game key of the unmodded game to an Item: tags, vouchers,
// packs, jokers, editions, bosses. Returns Item::RETRY if unknown.
Item itemFromGameKey(const std::string &key);
// The reverse: the game key of a tag/voucher/pack/joker/edition/boss, or "".
std::string gameKeyOf(Item item);
