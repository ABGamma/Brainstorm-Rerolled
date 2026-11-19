// gamedata.hpp — the item lists and odds a search predicts from.
//
// WHY
//   Balatro picks "item number N of an ordered list", where N comes from the
//   seed. The random numbers are the same in every copy of the game, but the
//   lists are not: mods add jokers, tags, vouchers, packs, rarities and
//   editions, and a deck or challenge changes what is available. With the
//   lists built into this library, a modded game was predicted wrongly.
//
// WHAT
//   A GameData holds everything the custom filters (filterprog.hpp) need to
//   know about the game the search is for:
//
//     - GameData::vanilla()  the unmodded game on a plain deck, built from
//                            the tables in items.hpp. Used when the mod
//                            sends nothing (and by the tests).
//     - GameData::parse()    the lists of the running game, sent as text by
//                            the mod (Brainstorm/Core/gamedata.lua), which
//                            asks the game itself for them.
//
//   Lists and numbers can be sent; code cannot. A mod that changes HOW the
//   game generates things (not just what exists) is still predicted wrongly;
//   the mod checks every found run against the prediction and says so.
//
// THE TEXT FORMAT
//   One record per line, words separated by spaces. Keys are the game's own
//   (G.P_CENTERS, G.P_TAGS, G.P_BLINDS). A key can itself contain a space
//   (Cryptid has "j_cry_Double Scale"), so every word is percent-encoded:
//   '%', spaces and control characters become %XX ("j_cry_Double%20Scale").
//   The filter text (filterprog.hpp) uses the same encoding for its keys.
//   In a list, a key may carry a mark:
//     key     available
//     key?    available only with a card of the matching enhancement in the
//             deck (the filter's `assume enhanced`)
//     key!    in the list but unavailable in a fresh run (the game re-rolls
//             when it lands on it)
//     -       an unavailable slot whose key is unknown
//
//     gamedata 1                         format version; must be the first line
//     pad 1                              1: an item that becomes unavailable
//                                        keeps its slot (stock game); 0: it
//                                        is removed and the list shrinks
//     tags <ante> <list>                 the tag list on that ante (1..8)
//     voucher <key> <start> <later> [required keys]
//                                        start: available in a fresh run;
//                                        later: available once all required
//                                        vouchers are bought. In list order.
//     firstpack <key>                    ante 1's first shop pack, when the
//                                        game always gives the same one
//     pack <from> <key>                  shop pack for a roll >= from (rolls
//                                        are 0..1; lines in rising order)
//     shop <total> <joker> <slots>       card type rates, cards per shop
//     raritypolls <n>                    rarity rolls made per shop joker
//     rarity <from> <stream> <list>      joker rarity for a roll >= from, the
//                                        name of its random stream without
//                                        the ante ("Joker1sho"), its jokers
//     edition <from> <key>               joker edition for a roll >= from
//                                        (below the first line: no edition;
//                                        "-" as key: no edition either)
//     soul <0|1>                         The Soul can appear in packs
//     blackhole <0|1>                    a Spectral card rolls for a Black
//                                        Hole (it changes which Spectral
//                                        cards are Souls); 1 if missing
//     legendary <stream> <list>          what The Soul gives ("Joker4")
//     boss <key> <antes>                 boss blinds, sorted by key; the
//                                        antes it can be the boss of, as
//                                        digits ("1234567", "8"; "-" = none)
//     editionstream <name>               the random stream of a shop joker's
//                                        edition, without the ante ("edisho")
//     shoprates <tarot> <planet> <playing> <spectral>
//                                        how often the shop offers the other
//                                        kinds of card (after jokers, in this
//                                        order; with <joker> they add up to
//                                        <total>). Missing: none of them
//     cards <Tarot|Planet|Spectral> <list>   (weighted games: with weights,
//                                        as poll_object weighs a pack card)
//                                        a kind of consumable card, as
//                                        get_current_pool gives it (The Soul
//                                        and Black Hole are in the Spectral
//                                        list, unavailable: they only come
//                                        from the rolls packs make for them)
//     modsoul <key> <rate> <repeat> <set>...
//                                        a mod's "legendary" consumable
//                                        (Steamodded's SMODS.Consumable
//                                        .legendaries, in their order): a
//                                        pack card of one of the sets (Tarot,
//                                        Planet, Spectral) rolls for it first,
//                                        on "soul_smods_" + set + ante, with
//                                        this chance; repeat 1: it can come
//                                        twice in one pack
//
// WEIGHTED GAMES ("weights 1")
//   A mod can switch on Steamodded's "object weights" (Pokermon does). The
//   game then no longer picks "item number N of a list": every item has a
//   weight, and a roll picks an item with a chance in proportion to it
//   (SMODS.poll_object in Steamodded's src/utils/weights.lua). The picks
//   also share random streams: the tags, vouchers, blinds and shop packs of
//   an ante all draw from one stream, in the order the game makes them, and
//   every shop card (jokers and consumables alike) draws from another.
//
//   In a list, the word "*<number>" sets the weight of the entries after it
//   ("*10 tag_a tag_b *2.5 tag_c"); it starts at 10. An unavailable item is
//   simply not in the list. "-" is a slot without an item: picking it makes
//   the game roll again (it has a weight too).
//
//     weights 1                          this is a weighted game
//     streams <type> <shop> <soul>       stream names without the ante: for
//                                        tags/vouchers/blinds/packs, for shop
//                                        cards, for what The Soul gives
//     vouchersperante <n>                vouchers the shop offers per ante
//     voucher ... / boss ...             as above, plus a "*<number>" word
//                                        with the item's weight
//     packlist <list>                    the shop packs (instead of `pack`)
//     shoptype <name> <rate> <list>      the kinds of shop card in the game's
//                                        order, how often each comes up, and
//                                        its items. "Joker" has no list (its
//                                        items are in the `rarity` lines); a
//                                        kind without a list draws nothing
//                                        (playing cards)
//     link <key> <list>                  while <key> is in the shop (or was
//                                        given by The Soul), the listed items
//                                        have these weights instead. An item
//                                        in the shop can't come again, so its
//                                        own weight is 0 then (not listed)
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct GameData {
  static constexpr int MAX_ANTE = 8;
  static constexpr int NONE = -1; // an unavailable slot / "no item"
  // What Steamodded gives an item that says nothing about its weight.
  static constexpr double DEFAULT_WEIGHT = 10;

  // Every key seen, numbered from 0; the rest of the struct uses the numbers.
  std::vector<std::string> keys;
  std::unordered_map<std::string, int> ids;
  int idOf(const std::string &key) const; // NONE if unknown
  const std::string &keyOf(int id) const; // "" for NONE

  // One slot of a list.
  struct Entry {
    enum State : unsigned char { AVAILABLE, GATED, UNAVAILABLE };
    int id = NONE; // NONE for "-"
    State state = AVAILABLE;
    double weight = DEFAULT_WEIGHT; // weighted games only
  };
  typedef std::vector<Entry> List;

  // A roll in [0, 1) -> a value: value[i] for from[i] <= roll < from[i + 1].
  // from[0] is always 0.
  struct Steps {
    std::vector<double> from;
    std::vector<int> value;
    // The step a roll falls in. Never goes down as the roll goes up, which is
    // what luaRandomDecide (util.hpp) needs; the values themselves may repeat.
    int index(double roll) const {
      int i = (int)from.size() - 1;
      while (i > 0 && roll < from[i]) i--;
      return i;
    }
  };

  bool pad = true;

  List tags[MAX_ANTE + 1]; // [1..MAX_ANTE]

  struct Voucher {
    int id = NONE;
    bool start = false; // available in a fresh run
    bool later = false; // available once everything in `needs` is bought
    std::vector<int> needs;
    double weight = DEFAULT_WEIGHT; // weighted games only
  };
  std::vector<Voucher> vouchers;

  int firstPack = NONE;
  Steps packs;

  double shopTotal = 28, shopJoker = 20;
  int shopSlots = 2;

  int rarityPolls = 1;
  Steps rarities; // value: index into `rarityPools`
  struct RarityPool {
    std::string stream; // e.g. "Joker1sho"; the ante is appended
    List jokers;
  };
  std::vector<RarityPool> rarityPools;

  Steps editions; // value: edition id, NONE = no edition

  bool soul = true;
  bool blackHole = true;
  std::string legendaryStream = "Joker4";
  List legendaries;

  struct Boss {
    int id = NONE;
    unsigned antes = 0; // bit a set: can be the boss of ante a (1..MAX_ANTE)
    double weight = DEFAULT_WEIGHT; // weighted games only
  };
  std::vector<Boss> bosses; // sorted by key, the order the game picks from

  std::string editionStream = "edisho"; // the ante is appended

  // Consumable cards: Tarot, Planet and Spectral (CARD_SETS order).
  static constexpr int CARD_SETS = 3;
  static const char *const CARD_SET_NAMES[CARD_SETS]; // "Tarot", "Planet", "Spectral"
  List cards[CARD_SETS];
  // The shop's rates for its other kinds of card (after jokers).
  double shopTarot = 0, shopPlanet = 0, shopPlaying = 0, shopSpectral = 0;
  // Mods' Soul-like consumables (see "modsoul" above).
  struct ModSoul {
    int id = NONE;
    double rate = 0.003;
    bool repeat = false;
    bool sets[CARD_SETS] = {}; // the sets of pack cards that roll for it
  };
  std::vector<ModSoul> modSouls;

  // --- Weighted games only (see "WEIGHTED GAMES" above) ---
  bool weighted = false;
  // Stream names; the ante is appended.
  std::string typeStream = "std_smods_polltype";
  std::string shopStream = "std_smods_pollsho";
  std::string soulStream = "std_smods_pollsou";
  int vouchersPerAnte = 1;
  List packList; // the shop packs, in the game's order
  struct ShopType {
    std::string name;
    double rate = 0;
    bool joker = false; // its items are the rarity lists
    List items;         // empty: this kind draws nothing
  };
  std::vector<ShopType> shopTypes; // in the game's order
  // links[id]: weights other items have while item `id` is around.
  struct Link {
    int id = NONE;
    double weight = 0;
  };
  std::vector<std::vector<Link>> links; // indexed by id; may be shorter than `keys`
  const std::vector<Link> *linksOf(int id) const {
    return id >= 0 && id < (int)links.size() && !links[id].empty() ? &links[id] : nullptr;
  }

  // Whether `id` is a pack the shop can offer at random.
  bool randomPack(int id) const;

  // Whether the game can pick boss number `index` on `ante`.
  bool bossEligible(size_t index, int ante) const {
    return ante >= 1 && ante <= MAX_ANTE && (bosses[index].antes >> ante & 1);
  }

  // The unmodded game on a plain deck (built once, shared).
  static std::shared_ptr<const GameData> vanilla();

  // Reads the text format. On a mistake returns null and sets `error` to a
  // message naming the line.
  static std::shared_ptr<const GameData> parse(const std::string &text,
                                               std::string &error);

  // Writes the text format (parse(toText()) gives the same data back).
  std::string toText() const;

  // One word of the text formats: '%', whitespace and control characters
  // as %XX (two hex digits), so a word never contains a space.
  static std::string encodeWord(const std::string &word);
  // The reverse; a '%' not followed by two hex digits is kept as it is.
  static std::string decodeWord(const std::string &word);

private:
  int intern(const std::string &key);
  bool parseList(const std::vector<std::string> &words,
                 const std::vector<std::string> &raw, size_t first, List &out);
  friend struct VanillaBuilder;
};
