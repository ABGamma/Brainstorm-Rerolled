// filterprog.cpp — see filterprog.hpp for the format and what it means.

#include "filterprog.hpp"

#include "fastscan.hpp"
#include "filters.hpp"
#include "functions.hpp"

#include <algorithm>
#include <optional>
#include <sstream>

// ---------------------------------------------------------------------------
// Game keys

namespace {

struct KeyItem {
  const char *key;
  Item item;
};

// Every joker's key, from the game's own G.P_CENTERS (generated from the
// running game, not typed by hand; the engine spells two names differently:
// the game's "Caino" is Item::Canio, "Seance" is Item::Seance).
const KeyItem JOKER_KEYS[] = {
    {"j_8_ball", Item::_8_Ball},
    {"j_abstract", Item::Abstract_Joker},
    {"j_acrobat", Item::Acrobat},
    {"j_ancient", Item::Ancient_Joker},
    {"j_arrowhead", Item::Arrowhead},
    {"j_astronomer", Item::Astronomer},
    {"j_banner", Item::Banner},
    {"j_baron", Item::Baron},
    {"j_baseball", Item::Baseball_Card},
    {"j_blackboard", Item::Blackboard},
    {"j_bloodstone", Item::Bloodstone},
    {"j_blue_joker", Item::Blue_Joker},
    {"j_blueprint", Item::Blueprint},
    {"j_bootstraps", Item::Bootstraps},
    {"j_brainstorm", Item::Brainstorm},
    {"j_bull", Item::Bull},
    {"j_burglar", Item::Burglar},
    {"j_burnt", Item::Burnt_Joker},
    {"j_business", Item::Business_Card},
    {"j_caino", Item::Canio},
    {"j_campfire", Item::Campfire},
    {"j_card_sharp", Item::Card_Sharp},
    {"j_cartomancer", Item::Cartomancer},
    {"j_castle", Item::Castle},
    {"j_cavendish", Item::Cavendish},
    {"j_ceremonial", Item::Ceremonial_Dagger},
    {"j_certificate", Item::Certificate},
    {"j_chaos", Item::Chaos_the_Clown},
    {"j_chicot", Item::Chicot},
    {"j_clever", Item::Clever_Joker},
    {"j_cloud_9", Item::Cloud_9},
    {"j_constellation", Item::Constellation},
    {"j_crafty", Item::Crafty_Joker},
    {"j_crazy", Item::Crazy_Joker},
    {"j_credit_card", Item::Credit_Card},
    {"j_delayed_grat", Item::Delayed_Gratification},
    {"j_devious", Item::Devious_Joker},
    {"j_diet_cola", Item::Diet_Cola},
    {"j_dna", Item::DNA},
    {"j_drivers_license", Item::Drivers_License},
    {"j_droll", Item::Droll_Joker},
    {"j_drunkard", Item::Drunkard},
    {"j_duo", Item::The_Duo},
    {"j_dusk", Item::Dusk},
    {"j_egg", Item::Egg},
    {"j_erosion", Item::Erosion},
    {"j_even_steven", Item::Even_Steven},
    {"j_faceless", Item::Faceless_Joker},
    {"j_family", Item::The_Family},
    {"j_fibonacci", Item::Fibonacci},
    {"j_flash", Item::Flash_Card},
    {"j_flower_pot", Item::Flower_Pot},
    {"j_fortune_teller", Item::Fortune_Teller},
    {"j_four_fingers", Item::Four_Fingers},
    {"j_gift", Item::Gift_Card},
    {"j_glass", Item::Glass_Joker},
    {"j_gluttenous_joker", Item::Gluttonous_Joker},
    {"j_golden", Item::Golden_Joker},
    {"j_greedy_joker", Item::Greedy_Joker},
    {"j_green_joker", Item::Green_Joker},
    {"j_gros_michel", Item::Gros_Michel},
    {"j_hack", Item::Hack},
    {"j_half", Item::Half_Joker},
    {"j_hallucination", Item::Hallucination},
    {"j_hanging_chad", Item::Hanging_Chad},
    {"j_hiker", Item::Hiker},
    {"j_hit_the_road", Item::Hit_the_Road},
    {"j_hologram", Item::Hologram},
    {"j_ice_cream", Item::Ice_Cream},
    {"j_idol", Item::The_Idol},
    {"j_invisible", Item::Invisible_Joker},
    {"j_joker", Item::Joker},
    {"j_jolly", Item::Jolly_Joker},
    {"j_juggler", Item::Juggler},
    {"j_loyalty_card", Item::Loyalty_Card},
    {"j_luchador", Item::Luchador},
    {"j_lucky_cat", Item::Lucky_Cat},
    {"j_lusty_joker", Item::Lusty_Joker},
    {"j_mad", Item::Mad_Joker},
    {"j_madness", Item::Madness},
    {"j_mail", Item::Mail_In_Rebate},
    {"j_marble", Item::Marble_Joker},
    {"j_matador", Item::Matador},
    {"j_merry_andy", Item::Merry_Andy},
    {"j_midas_mask", Item::Midas_Mask},
    {"j_mime", Item::Mime},
    {"j_misprint", Item::Misprint},
    {"j_mr_bones", Item::Mr_Bones},
    {"j_mystic_summit", Item::Mystic_Summit},
    {"j_obelisk", Item::Obelisk},
    {"j_odd_todd", Item::Odd_Todd},
    {"j_onyx_agate", Item::Onyx_Agate},
    {"j_oops", Item::Oops_All_6s},
    {"j_order", Item::The_Order},
    {"j_pareidolia", Item::Pareidolia},
    {"j_perkeo", Item::Perkeo},
    {"j_photograph", Item::Photograph},
    {"j_popcorn", Item::Popcorn},
    {"j_raised_fist", Item::Raised_Fist},
    {"j_ramen", Item::Ramen},
    {"j_red_card", Item::Red_Card},
    {"j_reserved_parking", Item::Reserved_Parking},
    {"j_ride_the_bus", Item::Ride_the_Bus},
    {"j_riff_raff", Item::Riff_raff},
    {"j_ring_master", Item::Showman},
    {"j_rocket", Item::Rocket},
    {"j_rough_gem", Item::Rough_Gem},
    {"j_runner", Item::Runner},
    {"j_satellite", Item::Satellite},
    {"j_scary_face", Item::Scary_Face},
    {"j_scholar", Item::Scholar},
    {"j_seance", Item::Seance},
    {"j_seeing_double", Item::Seeing_Double},
    {"j_selzer", Item::Seltzer},
    {"j_shoot_the_moon", Item::Shoot_the_Moon},
    {"j_shortcut", Item::Shortcut},
    {"j_sixth_sense", Item::Sixth_Sense},
    {"j_sly", Item::Sly_Joker},
    {"j_smeared", Item::Smeared_Joker},
    {"j_smiley", Item::Smiley_Face},
    {"j_sock_and_buskin", Item::Sock_and_Buskin},
    {"j_space", Item::Space_Joker},
    {"j_splash", Item::Splash},
    {"j_square", Item::Square_Joker},
    {"j_steel_joker", Item::Steel_Joker},
    {"j_stencil", Item::Joker_Stencil},
    {"j_stone", Item::Stone_Joker},
    {"j_stuntman", Item::Stuntman},
    {"j_supernova", Item::Supernova},
    {"j_superposition", Item::Superposition},
    {"j_swashbuckler", Item::Swashbuckler},
    {"j_throwback", Item::Throwback},
    {"j_ticket", Item::Golden_Ticket},
    {"j_to_the_moon", Item::To_the_Moon},
    {"j_todo_list", Item::To_Do_List},
    {"j_trading", Item::Trading_Card},
    {"j_tribe", Item::The_Tribe},
    {"j_triboulet", Item::Triboulet},
    {"j_trio", Item::The_Trio},
    {"j_troubadour", Item::Troubadour},
    {"j_trousers", Item::Spare_Trousers},
    {"j_turtle_bean", Item::Turtle_Bean},
    {"j_vagabond", Item::Vagabond},
    {"j_vampire", Item::Vampire},
    {"j_walkie_talkie", Item::Walkie_Talkie},
    {"j_wee", Item::Wee_Joker},
    {"j_wily", Item::Wily_Joker},
    {"j_wrathful_joker", Item::Wrathful_Joker},
    {"j_yorick", Item::Yorick},
    {"j_zany", Item::Zany_Joker},
};

const KeyItem EDITION_KEYS[] = {
    {"e_base", Item::No_Edition},   {"e_foil", Item::Foil},
    {"e_holo", Item::Holographic},  {"e_polychrome", Item::Polychrome},
    {"e_negative", Item::Negative},
};

// Boss blinds: the game's G.P_BLINDS keys, in BOSSES order (items.hpp).
const char *const BOSS_KEYS[] = {
    "bl_arm",         "bl_club",         "bl_eye",         "bl_final_acorn",
    "bl_final_bell",  "bl_final_heart",  "bl_final_leaf",  "bl_final_vessel",
    "bl_fish",        "bl_flint",        "bl_goad",        "bl_head",
    "bl_hook",        "bl_house",        "bl_manacle",     "bl_mark",
    "bl_mouth",       "bl_needle",       "bl_ox",          "bl_pillar",
    "bl_plant",       "bl_psychic",      "bl_serpent",     "bl_tooth",
    "bl_wall",        "bl_water",        "bl_wheel",       "bl_window"};
static_assert(sizeof(BOSS_KEYS) / sizeof(*BOSS_KEYS) == BOSSES.size(),
              "one key per boss");

template <class Pool> bool inPool(const Pool &pool, Item item) {
  for (const auto &i : pool)
    if (i == item) return true;
  return false;
}

bool isEdition(Item item) {
  for (const KeyItem &k : EDITION_KEYS)
    if (k.item == item) return true;
  return false;
}

// Jokers the game only offers once a card with the matching enhancement is
// in the deck (enhancement_gate in the game; locked by Instance::initLocks).
const Item ENHANCEMENT_JOKERS[] = {Item::Steel_Joker, Item::Stone_Joker,
                                   Item::Lucky_Cat, Item::Golden_Ticket,
                                   Item::Glass_Joker};
bool isEnhancementJoker(Item item) {
  for (Item i : ENHANCEMENT_JOKERS)
    if (i == item) return true;
  return false;
}

bool isPack(Item item) {
  for (const WeightedItem &w : PACKS)
    if (w.item == item && item != Item::RETRY) return true;
  return false;
}

} // namespace

Item itemFromGameKey(const std::string &key) {
  for (const KeyItem &k : JOKER_KEYS)
    if (key == k.key) return k.item;
  for (const KeyItem &k : EDITION_KEYS)
    if (key == k.key) return k.item;
  for (size_t i = 0; i < BOSSES.size(); i++)
    if (key == BOSS_KEYS[i]) return BOSSES[i];
  bool ok = true;
  Item item = itemFromKey(key, ok); // tags, vouchers, packs (filters.hpp)
  return ok ? item : Item::RETRY;
}

std::string gameKeyOf(Item item) {
  for (const KeyItem &k : JOKER_KEYS)
    if (k.item == item) return k.key;
  for (const KeyItem &k : EDITION_KEYS)
    if (k.item == item) return k.key;
  for (size_t i = 0; i < BOSSES.size(); i++)
    if (BOSSES[i] == item) return BOSS_KEYS[i];
  return keyFromItem(item);
}

// ---------------------------------------------------------------------------
// Parsing

namespace {

bool parseInt(const std::string &s, int lo, int hi, int &out) {
  if (s.empty() || s.size() > 4) return false;
  for (char c : s)
    if (c < '0' || c > '9') return false;
  out = std::stoi(s);
  return out >= lo && out <= hi;
}

// Reads one condition line. Returns "" on success, else what's wrong.
std::string parseLine(const std::vector<std::string> &words, Condition &c) {
  const std::string &kind = words[0];
  size_t opts = 1; // index of the first name=value word
  if (kind == "tag" || kind == "voucher" || kind == "pack" || kind == "joker" ||
      kind == "boss") {
    if (words.size() < 2) return "'" + kind + "' needs an item key";
    const std::string &key = words[1];
    Item item = itemFromGameKey(key);
    opts = 2;
    if (kind == "tag") {
      c.kind = Condition::TAG;
      if (!inPool(TAGS, item)) return "unknown tag '" + key + "'";
    } else if (kind == "voucher") {
      c.kind = Condition::VOUCHER;
      if (!inPool(VOUCHERS, item)) return "unknown voucher '" + key + "'";
    } else if (kind == "boss") {
      c.kind = Condition::BOSS;
      if (!inPool(BOSSES, item)) return "unknown boss '" + key + "'";
    } else if (kind == "pack") {
      c.kind = Condition::PACK;
      if (!isPack(item)) return "unknown pack '" + key + "'";
    } else {
      c.kind = Condition::JOKER;
      Item rarity = jokerRarity(item);
      if (rarity == Item::RETRY) return "unknown joker '" + key + "'";
      if (rarity == Item::Legendary)
        return "'" + key + "' is legendary and never appears in shops "
               "(use: soul legendary=" + key + ")";
    }
    c.item = item;
  } else if (kind == "soul") {
    c.kind = Condition::SOUL;
  } else {
    return "unknown condition '" + kind +
           "' (expected tag, voucher, pack, joker, soul or boss)";
  }

  for (size_t i = opts; i < words.size(); i++) {
    const std::string &w = words[i];
    size_t eq = w.find('=');
    if (eq == std::string::npos) return "expected name=value, got '" + w + "'";
    std::string name = w.substr(0, eq), value = w.substr(eq + 1);
    bool ok = true;
    if (name == "ante") {
      // "3" or a range "6-8".
      size_t dash = value.find('-');
      if (dash == std::string::npos) {
        ok = parseInt(value, 1, 8, c.ante);
        c.anteTo = c.ante;
      } else {
        ok = parseInt(value.substr(0, dash), 1, 8, c.ante) &&
             parseInt(value.substr(dash + 1), 1, 8, c.anteTo) &&
             c.ante <= c.anteTo;
      }
    } else if (name == "within" &&
               (c.kind == Condition::PACK || c.kind == Condition::JOKER)) {
      ok = parseInt(value, 1, 500, c.within);
    } else if (name == "blind" && c.kind == Condition::TAG) {
      c.blind = value == "any" ? 0 : value == "small" ? 1 : value == "big" ? 2 : -1;
      ok = c.blind >= 0;
    } else if (name == "edition" && c.kind == Condition::JOKER) {
      if (value != "any") {
        c.edition = itemFromGameKey(value);
        ok = isEdition(c.edition);
      }
    } else if (name == "packs" && c.kind == Condition::SOUL) {
      ok = parseInt(value, 1, 10, c.packs);
    } else if (name == "size" && c.kind == Condition::SOUL) {
      ok = (value == "3" || value == "5");
      if (ok) c.size = value[0] - '0';
    } else if (name == "legendary" && c.kind == Condition::SOUL) {
      if (value != "any") {
        c.item = itemFromGameKey(value);
        ok = inPool(LEGENDARY_JOKERS, c.item);
      }
    } else {
      return "'" + name + "' is not an option of '" + kind + "'";
    }
    if (!ok) return "bad value for " + name + ": '" + value + "'";
  }
  return "";
}

} // namespace

std::unique_ptr<FilterProgram> FilterProgram::parse(const std::string &text,
                                                    std::string &error) {
  std::unique_ptr<FilterProgram> prog(new FilterProgram());
  std::string normalised = text;
  std::replace(normalised.begin(), normalised.end(), ';', '\n');
  std::istringstream lines(normalised);
  std::string line;
  int lineNo = 0;
  while (std::getline(lines, line)) {
    lineNo++;
    std::istringstream in(line);
    std::vector<std::string> words;
    for (std::string w; in >> w;) words.push_back(w);
    if (words.empty() || words[0][0] == '#') continue;
    if (words[0] == "assume") {
      if (words.size() != 2 || words[1] != "enhanced") {
        error = "line " + std::to_string(lineNo) +
                ": expected 'assume enhanced'";
        return nullptr;
      }
      prog->assumeEnhanced = true;
      continue;
    }
    Condition c;
    std::string problem = parseLine(words, c);
    if (!problem.empty()) {
      error = "line " + std::to_string(lineNo) + ": " + problem;
      return nullptr;
    }
    if (prog->all.size() >= 64) {
      error = "too many conditions (at most 64)";
      return nullptr;
    }
    prog->all.push_back(c);
  }

  // Check order: cheapest kind first (the enum order is not that order).
  const Condition::Kind order[] = {Condition::BOSS, Condition::TAG,
                                   Condition::VOUCHER,
                                   Condition::PACK, Condition::SOUL,
                                   Condition::JOKER};
  for (Condition::Kind k : order)
    for (const Condition &c : prog->all)
      if (c.kind == k) prog->ordered.push_back(c);

  // Per-ante needs (see the class). A range needs every ante in it.
  for (const Condition &c : prog->all)
    for (int a = c.ante; a <= c.anteTo; a++) {
      switch (c.kind) {
      case Condition::TAG:
        prog->tagAnte[a] = true;
        prog->maxTagAnte = std::max(prog->maxTagAnte, a);
        break;
      case Condition::VOUCHER:
        prog->maxVoucherAnte = std::max(prog->maxVoucherAnte, a);
        break;
      case Condition::BOSS:
        prog->maxBossAnte = std::max(prog->maxBossAnte, a);
        break;
      case Condition::PACK: break;
      case Condition::JOKER:
        prog->jokerRarities[a] |= shopRarityBit(jokerRarity(c.item));
        prog->jokerEditions[a] |= c.edition != Item::RETRY;
        break;
      case Condition::SOUL:
        // Soul conditions on one ante share its packs, which only works if
        // they open packs of the same size.
        if (prog->soulSize[a] != 0 && prog->soulSize[a] != c.size) {
          error = "soul conditions in the same ante must use the same pack "
                  "size";
          return nullptr;
        }
        prog->soulSize[a] = c.size;
        break;
      }
    }

  // A boss condition that no ante of its range allows could never match
  // (e.g. a showdown boss before ante 8): say so instead of searching
  // forever.
  for (const Condition &c : prog->all) {
    if (c.kind != Condition::BOSS) continue;
    bool possible = false;
    for (int a = c.ante; a <= c.anteTo; a++) possible |= bossPossible(c.item, a);
    if (!possible) {
      std::string antes = c.ante == c.anteTo
                              ? "ante " + std::to_string(c.ante)
                              : "antes " + std::to_string(c.ante) + "-" +
                                    std::to_string(c.anteTo);
      int minAnte = 0;
      for (size_t i = 0; i < BOSSES.size(); i++)
        if (BOSSES[i] == c.item) minAnte = BOSS_MIN_ANTE[i];
      error = "'" + gameKeyOf(c.item) + "' can't be the boss of " + antes +
              (minAnte == 0 ? " (final bosses only appear on ante 8)"
                            : " (it appears from ante " +
                                  std::to_string(minAnte) +
                                  " on, but never on ante 8)");
      return nullptr;
    }
  }

  // Jokers that only appear with an enhanced card in the deck need
  // `assume enhanced` (anywhere in the text); Cavendish is never searchable.
  for (const Condition &c : prog->all) {
    if (c.kind != Condition::JOKER) continue;
    if (c.item == Item::Cavendish) {
      error = "'j_cavendish' only appears after Gros Michel dies, which "
              "can't be searched for";
      return nullptr;
    }
    if (isEnhancementJoker(c.item) && !prog->assumeEnhanced) {
      error = "'" + gameKeyOf(c.item) +
              "' only appears with an enhanced card in the deck: add "
              "'assume enhanced'";
      return nullptr;
    }
  }

  return prog;
}

// ---------------------------------------------------------------------------
// The reference meaning: each condition on its own, on a fresh run, with
// the full generators. Simple on purpose; see filterprog.hpp.

namespace {
// A run as it starts: run-start locks, as brainstormFilter() uses.
// With `assume enhanced`, the five enhancement jokers are in the pool from
// the start, for every condition (reference and fast check alike).
struct FreshRun {
  Instance inst;
  FreshRun(Seed &seed, bool assumeEnhanced) : inst(seed) {
    inst.initLocks(1, false, true);
    if (assumeEnhanced)
      for (Item j : ENHANCEMENT_JOKERS) inst.unlock(j);
  }
};
} // namespace

bool FilterProgram::referenceCheck(const Condition &c, Seed &seed) const {
  // A range holds if it holds in any of its antes.
  for (int a = c.ante; a <= c.anteTo; a++)
    if (referenceAt(c, a, seed)) return true;
  return false;
}

// One condition at one ante (c.ante/c.anteTo are ignored here).
bool FilterProgram::referenceAt(const Condition &cond, int ante,
                                Seed &seed) const {
  Condition c = cond;
  c.ante = ante;
  FreshRun run(seed, assumeEnhanced);
  Instance &inst = run.inst;
  switch (c.kind) {
  case Condition::TAG: {
    for (int a = 2; a <= c.ante; a++) inst.initUnlocks(a, false);
    Item small = inst.nextTag(c.ante), big = inst.nextTag(c.ante);
    return (c.blind != 2 && small == c.item) || (c.blind != 1 && big == c.item);
  }
  case Condition::VOUCHER: {
    for (int a = 1; a < c.ante; a++) inst.activateVoucher(inst.nextVoucher(a));
    return inst.nextVoucher(c.ante) == c.item;
  }
  case Condition::PACK: {
    // Ante 1's first shop always opens with a Buffoon Pack (no random draw);
    // from ante 2 on, that has already happened.
    if (c.ante >= 2) inst.cache.generatedFirstPack = true;
    for (int i = 0; i < c.within; i++)
      if (inst.nextPack(c.ante) == c.item) return true;
    return false;
  }
  case Condition::JOKER: {
    for (int i = 0; i < c.within; i++) {
      ShopItem it = inst.nextShopItem(c.ante);
      if (it.item == c.item &&
          (c.edition == Item::RETRY || it.jokerData.edition == c.edition))
        return true;
    }
    return false;
  }
  case Condition::SOUL: {
    for (int p = 0; p < c.packs; p++) {
      bool soul = false;
      for (Item it : inst.nextArcanaPack(c.size, c.ante))
        soul |= it == Item::The_Soul;
      if (!soul) return false;
      if (c.item != Item::RETRY &&
          inst.nextJoker(ItemSource::Soul, c.ante, true).joker != c.item)
        return false;
    }
    return true;
  }
  case Condition::BOSS: {
    // Every ante's boss depends on the bosses before it.
    for (int a = 1; a < c.ante; a++) inst.nextBoss(a);
    return inst.nextBoss(c.ante) == c.item;
  }
  }
  return false;
}

// ---------------------------------------------------------------------------
// The fast check. Each kind uses its own random streams (tags: "Tag<ante>",
// packs: "shop_pack<ante>", ...), so the kinds can share one Instance and be
// checked in any order. What a seed offers in an ante is worked out on first
// use and kept (SeedFacts); state that carries over between antes (tag
// unlocks, bought vouchers) is built up in ante order, as the reference
// builds it.

// Everything the fast check has learned about one seed so far. One per
// thread, reused from seed to seed so no memory is allocated per seed.
struct SeedFacts {
  const FilterProgram *prog = nullptr;
  Seed *seed = nullptr;
  Instance *inst = nullptr; // tags, packs, souls, jokers
  static const int A = FilterProgram::MAX_ANTE + 1;

  bool tagsDone = false;
  Item small[A], big[A];

  bool vouchersDone = false;
  Item voucher[A];

  bool bossesDone = false;
  Item boss[A];

  bool packsStarted = false;
  std::vector<Item> packs[A];

  int souls[A];        // packs opened so far in that ante, each with a Soul
  bool soulsEnded[A];  // a pack without a Soul was opened: no more count

  std::vector<ShopJokerScan::Joker> shop[A];
  std::optional<ShopJokerScan> scan[A];

  void reset(const FilterProgram *p, Seed *s, Instance *i) {
    prog = p, seed = s, inst = i;
    tagsDone = vouchersDone = bossesDone = packsStarted = false;
    for (int a = 0; a < A; a++) {
      packs[a].clear();
      shop[a].clear();
      scan[a].reset();
      souls[a] = 0;
      soulsEnded[a] = false;
    }
  }

  void needTags() {
    if (tagsDone) return;
    tagsDone = true;
    // Unlocks in ante order (ante 2 unlocks more tags), as the reference.
    for (int a = 1; a <= prog->maxTagAnte; a++) {
      if (a >= 2) inst->initUnlocks(a, false);
      if (prog->tagAnte[a]) {
        small[a] = inst->nextTag(a);
        big[a] = inst->nextTag(a);
      }
    }
  }

  void needVouchers() {
    if (vouchersDone) return;
    vouchersDone = true;
    // Buying vouchers changes shop odds and editions, which the other kinds
    // assume untouched, so the voucher chain runs on its own Instance.
    FreshRun run(*seed, prog->assumeEnhanced);
    for (int a = 1; a <= prog->maxVoucherAnte; a++) {
      voucher[a] = run.inst.nextVoucher(a);
      run.inst.activateVoucher(voucher[a]);
    }
  }

  void needBosses() {
    if (bossesDone) return;
    bossesDone = true;
    // The "boss" stream and the boss usage counts are used by nothing else,
    // so the shared Instance is fine.
    for (int a = 1; a <= prog->maxBossAnte; a++) boss[a] = inst->nextBoss(a);
  }

  // The first n packs offered in ante a.
  const std::vector<Item> &needPacks(int a, int n) {
    if (!packsStarted) {
      // Ante 1's first shop always opens with a Buffoon Pack, without a
      // random draw (Instance::nextPack, game 1.0.1+). Handled here per
      // ante, so antes can be looked at in any order.
      packsStarted = true;
      inst->cache.generatedFirstPack = true;
    }
    std::vector<Item> &list = packs[a];
    if (list.empty() && a == 1 && inst->params.version > 10099)
      list.push_back(Item::Buffoon_Pack);
    while ((int)list.size() < n) list.push_back(inst->nextPack(a));
    return list;
  }

  // How many packs in a row (up to n) contain The Soul in ante a.
  int needSouls(int a, int n) {
    while (souls[a] < n && !soulsEnded[a]) {
      if (arcanaPackHasSoul(*inst, prog->soulSize[a], a)) souls[a]++;
      else soulsEnded[a] = true;
    }
    return souls[a];
  }

  // The first n shop cards of ante a (reduced to the jokers asked about).
  const std::vector<ShopJokerScan::Joker> &needShop(int a, int n) {
    if (!scan[a])
      scan[a].emplace(*inst, a, prog->jokerRarities[a], prog->jokerEditions[a]);
    std::vector<ShopJokerScan::Joker> &list = shop[a];
    while ((int)list.size() < n) list.push_back(scan[a]->next());
    return list;
  }

  bool holdsAt(const Condition &c, int a) {
    switch (c.kind) {
    case Condition::TAG:
      needTags();
      return (c.blind != 2 && small[a] == c.item) ||
             (c.blind != 1 && big[a] == c.item);
    case Condition::VOUCHER:
      needVouchers();
      return voucher[a] == c.item;
    case Condition::BOSS:
      needBosses();
      return boss[a] == c.item;
    case Condition::PACK: {
      const std::vector<Item> &list = needPacks(a, c.within);
      return std::find(list.begin(), list.begin() + c.within, c.item) !=
             list.begin() + c.within;
    }
    case Condition::SOUL:
      if (needSouls(a, c.packs) < c.packs) return false;
      // Which legendary: rare (only reached when every Soul is there), so
      // simply ask the reference.
      return c.item == Item::RETRY || prog->referenceAt(c, a, *seed);
    case Condition::JOKER: {
      const auto &list = needShop(a, c.within);
      for (int i = 0; i < c.within; i++)
        if (list[i].joker == c.item &&
            (c.edition == Item::RETRY || list[i].edition == c.edition))
          return true;
      return false;
    }
    }
    return false;
  }
};

bool FilterProgram::matches(Seed &seed) const {
  FreshRun run(seed, assumeEnhanced);
  thread_local SeedFacts facts;
  facts.reset(this, &seed, &run.inst);
  for (const Condition &c : ordered) {
    bool holds = false;
    for (int a = c.ante; a <= c.anteTo && !holds; a++) holds = facts.holdsAt(c, a);
    if (!holds) {
      facts.reset(nullptr, nullptr, nullptr); // drop references to `run`
      return false;
    }
  }
  facts.reset(nullptr, nullptr, nullptr);
  return true;
}
