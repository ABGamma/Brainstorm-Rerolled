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

// Consumables: the game's G.P_CENTERS keys (note the game's own spelling
// "c_heirophant").
const KeyItem CARD_KEYS[] = {
    {"c_fool", Item::The_Fool},
    {"c_magician", Item::The_Magician},
    {"c_high_priestess", Item::The_High_Priestess},
    {"c_empress", Item::The_Empress},
    {"c_emperor", Item::The_Emperor},
    {"c_heirophant", Item::The_Hierophant},
    {"c_lovers", Item::The_Lovers},
    {"c_chariot", Item::The_Chariot},
    {"c_justice", Item::Justice},
    {"c_hermit", Item::The_Hermit},
    {"c_wheel_of_fortune", Item::The_Wheel_of_Fortune},
    {"c_strength", Item::Strength},
    {"c_hanged_man", Item::The_Hanged_Man},
    {"c_death", Item::Death},
    {"c_temperance", Item::Temperance},
    {"c_devil", Item::The_Devil},
    {"c_tower", Item::The_Tower},
    {"c_star", Item::The_Star},
    {"c_moon", Item::The_Moon},
    {"c_sun", Item::The_Sun},
    {"c_judgement", Item::Judgement},
    {"c_world", Item::The_World},
    {"c_mercury", Item::Mercury},
    {"c_venus", Item::Venus},
    {"c_earth", Item::Earth},
    {"c_mars", Item::Mars},
    {"c_jupiter", Item::Jupiter},
    {"c_saturn", Item::Saturn},
    {"c_uranus", Item::Uranus},
    {"c_neptune", Item::Neptune},
    {"c_pluto", Item::Pluto},
    {"c_planet_x", Item::Planet_X},
    {"c_ceres", Item::Ceres},
    {"c_eris", Item::Eris},
    {"c_familiar", Item::Familiar},
    {"c_grim", Item::Grim},
    {"c_incantation", Item::Incantation},
    {"c_talisman", Item::Talisman},
    {"c_aura", Item::Aura},
    {"c_wraith", Item::Wraith},
    {"c_sigil", Item::Sigil},
    {"c_ouija", Item::Ouija},
    {"c_ectoplasm", Item::Ectoplasm},
    {"c_immolate", Item::Immolate},
    {"c_ankh", Item::Ankh},
    {"c_deja_vu", Item::Deja_Vu},
    {"c_hex", Item::Hex},
    {"c_trance", Item::Trance},
    {"c_medium", Item::Medium},
    {"c_cryptid", Item::Cryptid},
    {"c_soul", Item::The_Soul},
    {"c_black_hole", Item::Black_Hole},
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

// Jokers the game only offers once a card with the matching enhancement is
// in the deck (enhancement_gate in the game; locked by Instance::initLocks).
const Item ENHANCEMENT_JOKERS[] = {Item::Steel_Joker, Item::Stone_Joker,
                                   Item::Lucky_Cat, Item::Golden_Ticket,
                                   Item::Glass_Joker};
} // namespace

Item itemFromGameKey(const std::string &key) {
  for (const KeyItem &k : JOKER_KEYS)
    if (key == k.key) return k.item;
  for (const KeyItem &k : EDITION_KEYS)
    if (key == k.key) return k.item;
  for (const KeyItem &k : CARD_KEYS)
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
  for (const KeyItem &k : CARD_KEYS)
    if (k.item == item) return k.key;
  for (size_t i = 0; i < BOSSES.size(); i++)
    if (BOSSES[i] == item) return BOSS_KEYS[i];
  return keyFromItem(item);
}

// ---------------------------------------------------------------------------
// Parsing

namespace {

const int NONE = GameData::NONE;

bool parseInt(const std::string &s, int lo, int hi, int &out) {
  if (s.empty() || s.size() > 4) return false;
  for (char c : s)
    if (c < '0' || c > '9') return false;
  out = std::stoi(s);
  return out >= lo && out <= hi;
}

// The entry for `id` in a list, or null.
const GameData::Entry *find(const GameData::List &list, int id) {
  for (const GameData::Entry &e : list)
    if (e.id == id) return &e;
  return nullptr;
}

bool contains(const std::vector<int> &v, int id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}

// Reads one condition line. Returns "" on success, else what's wrong.
std::string parseLine(const std::vector<std::string> &words, Condition &c,
                      const GameData &d) {
  const std::string &kind = words[0];
  size_t opts = 1; // index of the first name=value word
  if (kind == "tag" || kind == "voucher" || kind == "pack" || kind == "joker" ||
      kind == "boss") {
    if (words.size() < 2) return "'" + kind + "' needs an item key";
    const std::string &key = words[1];
    int id = d.idOf(key);
    opts = 2;
    bool known = false;
    if (kind == "tag") {
      c.kind = Condition::TAG;
      for (int a = 1; a <= GameData::MAX_ANTE; a++)
        known |= id != NONE && find(d.tags[a], id);
    } else if (kind == "voucher") {
      c.kind = Condition::VOUCHER;
      for (const GameData::Voucher &v : d.vouchers) known |= v.id == id && id != NONE;
    } else if (kind == "boss") {
      c.kind = Condition::BOSS;
      for (const GameData::Boss &b : d.bosses) known |= b.id == id && id != NONE;
    } else if (kind == "pack") {
      c.kind = Condition::PACK;
      known = id != NONE && (id == d.firstPack || d.randomPack(id));
    } else {
      c.kind = Condition::JOKER;
      for (const GameData::RarityPool &p : d.rarityPools)
        known |= id != NONE && find(p.jokers, id);
      if (!known && id != NONE && find(d.legendaries, id))
        return "'" + key + "' is legendary and never appears in shops "
               "(use: soul legendary=" + key + ")";
    }
    if (!known) return "unknown " + kind + " '" + key + "'";
    c.id = id;
    c.item = itemFromGameKey(key);
  } else if (kind == "soul") {
    c.kind = Condition::SOUL;
  } else if (kind == "card") {
    if (words.size() < 2) return "'card' needs an item key";
    const std::string &key = words[1];
    opts = 2;
    c.kind = Condition::CARD;
    c.id = d.idOf(key);
    c.set = -1;
    for (int i = 0; i < GameData::CARD_SETS && c.id != NONE; i++)
      if (find(d.cards[i], c.id)) c.set = i;
    // A weighted game's lists leave out cards that only come from rolls:
    // The Soul, Black Hole and mods' Soul-like cards.
    if (c.set < 0 && c.id != NONE && (key == "c_soul" || key == "c_black_hole"))
      c.set = 2;
    for (const GameData::ModSoul &m : d.modSouls)
      for (int i = 0; i < GameData::CARD_SETS && c.set < 0 && m.id == c.id && c.id != NONE; i++)
        if (m.sets[i]) c.set = i;
    if (c.set < 0) return "unknown card '" + key + "'";
    c.item = itemFromGameKey(key);
    // The pack of its kind; The Soul and Black Hole (Spectral cards that
    // only come from rolls) default to the pack they come from most.
    c.from = key == "c_soul" ? Condition::ARCANA
             : key == "c_black_hole" ? Condition::CELESTIAL
                                     : c.set + 1;
  } else {
    return "unknown condition '" + kind +
           "' (expected tag, voucher, pack, joker, soul, card or boss)";
  }

  bool sizeGiven = false, withinGiven = false, packsGiven = false, fromGiven = false;
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
    } else if (name == "within" && (c.kind == Condition::PACK || c.kind == Condition::JOKER ||
                                    c.kind == Condition::CARD)) {
      ok = parseInt(value, 1, 500, c.within);
      withinGiven = true;
    } else if (name == "blind" && c.kind == Condition::TAG) {
      c.blind = value == "any" ? 0 : value == "small" ? 1 : value == "big" ? 2 : -1;
      ok = c.blind >= 0;
    } else if (name == "edition" && (c.kind == Condition::JOKER||c.kind == Condition::SOUL)) {
      if (value == "any") {
        c.anyEdition = true;
      } else if (value == "e_base") {
        c.anyEdition = false, c.editionId = NONE, c.edition = Item::No_Edition;
      } else {
        c.anyEdition = false;
        c.editionId = d.idOf(value);
        c.edition = itemFromGameKey(value);
        ok = c.editionId != NONE && contains(d.editions.value, c.editionId);
      }
    } else if (name == "packs" && (c.kind == Condition::SOUL || c.kind == Condition::JOKER ||
                                   c.kind == Condition::CARD)) {
      ok = parseInt(value, 1, 10, c.packs);
      packsGiven = true;
    } else if (name == "size" && (c.kind == Condition::SOUL || c.kind == Condition::JOKER ||
                                  c.kind == Condition::CARD)) {
      ok = value.size() == 1 && value[0] >= '2' && value[0] <= '5';
      if (ok) c.size = value[0] - '0', sizeGiven = true;
    } else if (name == "from" && c.kind == Condition::SOUL) {
      ok = value == "arcana" || value == "spectral";
      c.spectral = value == "spectral";
    } else if (name == "from" && c.kind == Condition::CARD) {
      static const char *const FROM[] = {"shop", "arcana", "celestial", "spectral"};
      fromGiven = true;
      ok = false;
      for (int f = 0; f < 4; f++)
        if (value == FROM[f]) c.from = f, ok = true;
    } else if (name == "from" && c.kind == Condition::JOKER) {
      ok = value == "shop" || value == "buffoon";
      c.buffoon = value == "buffoon";
    } else if (name == "legendary" && c.kind == Condition::SOUL) {
      if (value != "any") {
        c.id = d.idOf(value);
        c.item = itemFromGameKey(value);
        ok = c.id != NONE && find(d.legendaries, c.id);
      }
    } else {
      return "'" + name + "' is not an option of '" + kind + "'";
    }
    if (!ok) return "bad value for " + name + ": '" + value + "'";
  }
  // Shop jokers count shop cards (within); Buffoon pack jokers count packs.
  if (c.kind == Condition::JOKER && !c.buffoon && (packsGiven || sizeGiven))
    return std::string("'") + (packsGiven ? "packs" : "size") +
           "' is for jokers in Buffoon packs (add from=buffoon)";
  if (c.kind == Condition::JOKER && c.buffoon) {
    if (withinGiven)
      return "'within' is for shop jokers; for Buffoon packs use packs= and size=";
    if (!sizeGiven) c.size = 2;
    if (c.size != 2 && c.size != 4)
      return "bad value for size: '" + std::to_string(c.size) +
             "' (Buffoon packs have 2 or 4 cards)";
  }
  if (c.kind == Condition::CARD) {
    static const char *const PACK_NAMES[] = {"the shop", "Arcana packs", "Celestial packs",
                                             "Spectral packs"};
    // Where each kind of card can come from: its own pack and the shop;
    // The Soul from Arcana and Spectral packs, Black Hole from Celestial
    // and Spectral packs (Omen Globe not assumed).
    const std::string &key = words[1];
    // A mod's Soul-like card: the packs whose cards roll for it.
    const GameData::ModSoul *mod = nullptr;
    for (const GameData::ModSoul &m : d.modSouls)
      if (m.id == c.id) mod = &m;
    if (mod && !fromGiven)
      for (int f = Condition::SPECTRAL; f >= Condition::ARCANA; f--)
        if (mod->sets[f - 1]) c.from = f;
    bool fits = mod ? c.from != Condition::SHOP && mod->sets[c.from - 1]
                : key == "c_soul"         ? c.from == Condition::ARCANA || c.from == Condition::SPECTRAL
                : key == "c_black_hole" ? c.from == Condition::CELESTIAL || c.from == Condition::SPECTRAL
                                        : c.from == Condition::SHOP || c.from == c.set + 1;
    if (!fits) return "'" + key + "' can't come from " + PACK_NAMES[c.from];
    if (c.from == Condition::SHOP && (packsGiven || sizeGiven))
      return std::string("'") + (packsGiven ? "packs" : "size") +
             "' is for cards in packs; in the shop use within=";
    if (c.from != Condition::SHOP && withinGiven)
      return "'within' is for cards in the shop; in packs use packs= and size=";
    bool spectral = c.from == Condition::SPECTRAL;
    if (!sizeGiven) c.size = spectral ? 4 : 5;
    bool sized = spectral ? (c.size == 2 || c.size == 4) : (c.size == 3 || c.size == 5);
    if (c.from != Condition::SHOP && !sized)
      return "bad value for size: '" + std::to_string(c.size) + "' (" + PACK_NAMES[c.from] +
             (spectral ? " have 2 or 4 cards)" : " have 3 or 5 cards)");
  }
  // Pack sizes: Arcana 3 or 5 cards, Spectral 2 or 4.
  if (c.kind == Condition::SOUL) {
    if (c.spectral && !sizeGiven) c.size = 4;
    bool fits = c.spectral ? (c.size == 2 || c.size == 4) : (c.size == 3 || c.size == 5);
    if (!fits)
      return "bad value for size: '" + std::to_string(c.size) + "' (" +
             (c.spectral ? "Spectral packs have 2 or 4 cards" : "Arcana packs have 3 or 5 cards") + ")";
  }
  return "";
}

std::string anteWords(const Condition &c) {
  return c.ante == c.anteTo ? "ante " + std::to_string(c.ante)
                            : "antes " + std::to_string(c.ante) + "-" +
                                  std::to_string(c.anteTo);
}

} // namespace

// A shop stream name ("Joker1sho", "edisho") as Buffoon packs use it
// ("Joker1buf", "edibuf").
static std::string toBuffoon(const std::string &shopName) {
  const std::string &sho = ItemSource::Shop;
  if (shopName.size() >= sho.size() &&
      shopName.compare(shopName.size() - sho.size(), sho.size(), sho) == 0)
    return shopName.substr(0, shopName.size() - sho.size()) + ItemSource::Buffoon_Pack;
  return shopName + ItemSource::Buffoon_Pack;
}

void FilterProgram::StreamKeys::set(const std::string &name) {
  base = name;
  for (int r = 2; r <= 9; r++) re[r - 2] = name + "_resample" + anteToString(r);
}

// Turns the lists of `gd` into what the fast check reads: plain ids with
// NONE in the unavailable slots, and every stream name it will ask for.
void FilterProgram::resolve() {
  const GameData &d = *gd;
  auto ids = [&](const GameData::List &list) {
    std::vector<int> out;
    for (const GameData::Entry &e : list) {
      bool ok = e.state == GameData::Entry::AVAILABLE ||
                (e.state == GameData::Entry::GATED && assumeEnhanced);
      out.push_back(ok ? e.id : NONE);
    }
    return out;
  };
  jokerPool.clear();
  for (const GameData::RarityPool &p : d.rarityPools) jokerPool.push_back(ids(p.jokers));
  legendaryPool = ids(d.legendaries);
  legendaryKeys.set(d.legendaryStream);

  voucherIndex.assign(d.keys.size(), -1);
  voucherUnlocks.assign(d.vouchers.size(), {});
  for (size_t i = 0; i < d.vouchers.size(); i++) voucherIndex[d.vouchers[i].id] = (int)i;
  for (size_t i = 0; i < d.vouchers.size(); i++)
    for (int need : d.vouchers[i].needs)
      if (voucherIndex[need] >= 0) voucherUnlocks[voucherIndex[need]].push_back((int)i);

  for (int a = 1; a <= MAX_ANTE; a++) {
    std::string ante = anteToString(a);
    tagPool[a] = ids(d.tags[a]);
    // The names the game builds: get_next_tag_key, get_next_voucher_key,
    // get_pack, create_card_for_shop and create_card (common_events.lua).
    tagKeys[a].set(RandomType::Tags + ante);
    voucherKeys[a].set(RandomType::Voucher + ante);
    packKey[a] = RandomType::Shop_Pack + ante;
    cdtKey[a] = RandomType::Card_Type + ante;
    rarityKey[a] = RandomType::Joker_Rarity + ante + ItemSource::Shop;
    editionKey[a] = d.editionStream + ante;
    soulKey[a] = RandomType::Soul + RandomType::Tarot + ante;
    soulEditionKey[a] = RandomType::Joker_Edition + ItemSource::Soul + ante;
    soulSpectralKey[a] = RandomType::Soul + RandomType::Spectral + ante;
    jokerKeys[a].assign(d.rarityPools.size(), {});
    for (size_t r = 0; r < d.rarityPools.size(); r++)
      jokerKeys[a][r].set(d.rarityPools[r].stream + ante);
    jokerRarities[a].assign(d.rarityPools.size(), 0);
    // Buffoon packs: create_card(..., 'buf') where the shop passes 'sho';
    // the game puts that word on the end of the stream names.
    bufRarityKey[a] = RandomType::Joker_Rarity + ante + ItemSource::Buffoon_Pack;
    bufEditionKey[a] = toBuffoon(d.editionStream) + ante;
    bufJokerKeys[a].assign(d.rarityPools.size(), {});
    for (size_t r = 0; r < d.rarityPools.size(); r++)
      bufJokerKeys[a][r].set(toBuffoon(d.rarityPools[r].stream) + ante);
    bufRarities[a].assign(d.rarityPools.size(), 0);
    // Consumables: create_card(set, ..., key_append) in the game; the packs
    // pass 'ar1', 'pl1', 'spe', the shop 'sho' (Instance::nextTarot, ...).
    static const std::string PACK_APPEND[4] = {"", ItemSource::Arcana_Pack,
                                               ItemSource::Celestial_Pack,
                                               ItemSource::Spectral_Pack};
    static const std::string ROLL_SET[4] = {"", RandomType::Tarot, RandomType::Planet,
                                            RandomType::Spectral};
    for (int f = Condition::ARCANA; f <= Condition::SPECTRAL; f++) {
      cardPackKeys[a][f].set(GameData::CARD_SET_NAMES[f - 1] + PACK_APPEND[f] + ante);
      cardRollKey[a][f] = RandomType::Soul + ROLL_SET[f] + ante;
      cardModRollKey[a][f] = "soul_smods_" + ROLL_SET[f] + ante;
    }
    for (int i = 0; i < GameData::CARD_SETS; i++)
      cardShopKeys[a][i].set(GameData::CARD_SET_NAMES[i] + ItemSource::Shop + ante);
  }
  for (int i = 0; i < GameData::CARD_SETS; i++) cardPool[i] = ids(d.cards[i]);
  for (int f = Condition::ARCANA; f <= Condition::SPECTRAL; f++) {
    modSouls[f].clear();
    for (const GameData::ModSoul &m : d.modSouls)
      if (m.sets[f - 1]) modSouls[f].push_back(m);
  }
  soulCard = d.idOf("c_soul");
  holeCard = d.idOf("c_black_hole");

  if (!d.weighted) return;
  for (int a = 1; a <= MAX_ANTE; a++) {
    std::string ante = anteToString(a);
    wTags[a] = makePool(d.tags[a], false);
    typeKey[a] = d.typeStream + ante;
    shopKey[a] = d.shopStream + ante;
    soulDrawKey[a] = d.soulStream + ante;
  }
  wPacks = makePool(d.packList, false);
  wCards.clear();
  for (int i = 0; i < GameData::CARD_SETS; i++) wCards.push_back(makePool(d.cards[i], true));
  // Pack cards: poll_object with the pack's key_append where the shop's
  // stream has "sho" ("std_smods_pollsho" -> "std_smods_pollar1").
  static const std::string APPEND[4] = {"", ItemSource::Arcana_Pack, ItemSource::Celestial_Pack,
                                        ItemSource::Spectral_Pack};
  std::string base = d.shopStream;
  const std::string &sho = ItemSource::Shop;
  if (base.size() >= sho.size() && base.compare(base.size() - sho.size(), sho.size(), sho) == 0)
    base.resize(base.size() - sho.size());
  for (int a = 1; a <= MAX_ANTE; a++)
    for (int f = Condition::ARCANA; f <= Condition::SPECTRAL; f++)
      cardPackStream[a][f] = base + APPEND[f] + anteToString(a);
  for (int a = 1; a <= MAX_ANTE; a++)
    bufStream[a] = base + ItemSource::Buffoon_Pack + anteToString(a);
  wLegendary = makePool(d.legendaries, true);
  wRarity.clear();
  for (const GameData::RarityPool &p : d.rarityPools) wRarity.push_back(makePool(p.jokers, true));
  wTypes.clear();
  wTypeCum.clear();
  double total = 0;
  for (const GameData::ShopType &t : d.shopTypes) {
    wTypes.push_back(makePool(t.items, true));
    total += t.rate; // the same additions, in the same order, as the game's
    wTypeCum.push_back(total);
  }
}

// A list of a weighted game as the fast check reads it. An item that needs
// an enhanced card (and `assume enhanced` is off) has weight 0: it can't be
// picked, which is what leaving it out of the list does in the game.
FilterProgram::WPool FilterProgram::makePool(const GameData::List &list,
                                             bool changing) const {
  WPool p;
  double total = 0;
  for (const GameData::Entry &e : list) {
    bool open = e.state == GameData::Entry::AVAILABLE ||
                (e.state == GameData::Entry::GATED && assumeEnhanced);
    double w = (e.id == NONE || open) ? e.weight : 0;
    p.id.push_back(e.id);
    p.w.push_back(w);
    total += w;
    p.cum.push_back(total);
  }
  if (changing) {
    size_t keys = gd->keys.size();
    p.indexOf.assign(keys, -1);
    p.affected.assign(keys, 0);
    for (size_t i = 0; i < p.id.size(); i++)
      if (p.id[i] != NONE) p.indexOf[p.id[i]] = (int)i;
    for (size_t id = 0; id < keys; id++) {
      if (p.indexOf[id] >= 0) p.affected[id] = 1;
      if (const std::vector<GameData::Link> *links = gd->linksOf((int)id))
        for (const GameData::Link &l : *links)
          if (p.indexOf[l.id] >= 0) p.affected[id] = 1;
    }
  }
  return p;
}

// Why a condition can never hold in a fresh run, or "" if it can. A search
// for it would go through every seed there is without a match.
std::string FilterProgram::impossible(const Condition &c) const {
  const GameData &d = *gd;
  const std::string key = "'" + d.keyOf(c.id) + "'";
  switch (c.kind) {
  case Condition::TAG: {
    int first = 0;
    for (int a = MAX_ANTE; a >= 1; a--)
      if (contains(tagPool[a], c.id)) first = a;
    for (int a = c.ante; a <= c.anteTo; a++)
      if (contains(tagPool[a], c.id)) return "";
    return key + " can't be a skip tag on " + anteWords(c) +
           (first == 0 ? " (it is not available in a fresh run)"
                       : " (it is available from ante " + std::to_string(first) + " on)");
  }
  case Condition::VOUCHER: {
    // The earliest ante a voucher can be in the shop: 1 if it is available
    // from the start, else one ante after the vouchers it needs.
    std::vector<int> earliest(d.vouchers.size(), 0); // 0 = not worked out
    for (int round = 0; round < MAX_ANTE; round++)
      for (size_t i = 0; i < d.vouchers.size(); i++) {
        const GameData::Voucher &v = d.vouchers[i];
        if (earliest[i]) continue;
        if (v.start) {
          earliest[i] = 1;
        } else if (v.later && !v.needs.empty()) {
          int after = 0;
          for (int need : v.needs) {
            int e = voucherIndex[need] >= 0 ? earliest[voucherIndex[need]] : 0;
            after = (e == 0 || after < 0) ? -1 : std::max(after, e);
          }
          if (after > 0) earliest[i] = after + 1;
        }
      }
    int e = earliest[voucherIndex[c.id]];
    if (e != 0 && e <= c.anteTo) return "";
    if (e == 0) return key + " can't be in the shop of a fresh run";
    std::string needs;
    for (int need : d.vouchers[voucherIndex[c.id]].needs)
      needs += (needs.empty() ? "" : ", ") + d.keyOf(need);
    return key + " can't be in the shop on " + anteWords(c) + " (it needs " +
           needs + " bought first, so ante " + std::to_string(e) +
           " at the earliest)";
  }
  case Condition::PACK: {
    bool random = d.randomPack(c.id);
    for (int a = c.ante; a <= c.anteTo; a++) {
      bool fixedFirst = a == 1 && d.firstPack != NONE;
      if (fixedFirst ? (c.id == d.firstPack || (random && c.within >= 2)) : random)
        return "";
    }
    if (!random && c.ante > 1)
      return key + " is only the first pack of ante 1";
    return key + " can't be among the first " + std::to_string(c.within) +
           " packs of " + anteWords(c) + " (the first pack of ante 1 is always " +
           d.keyOf(d.firstPack) + ")";
  }
  case Condition::CARD: {
    if (c.from == Condition::SHOP) {
      double rate = c.set == 0 ? d.shopTarot : c.set == 1 ? d.shopPlanet : d.shopSpectral;
      if (d.weighted) { // the shop's kinds and rates, as poll_object_type has them
        rate = 0;
        for (const GameData::ShopType &t : d.shopTypes)
          if (t.name == GameData::CARD_SET_NAMES[c.set] && !t.items.empty()) rate = t.rate;
      }
      if (rate <= 0)
        return key + " can't be in the shop of a fresh run (" +
               (c.set == 2 ? "Spectral cards are only sold with the Ghost Deck)"
                           : "this game's shop has no cards of its kind)");
    }
    if (c.id == soulCard) return d.soul ? "" : "The Soul can't appear in this run";
    for (const GameData::ModSoul &m : d.modSouls)
      if (m.id == c.id) return d.soul ? "" : key + " can't appear in this run";
    if (c.id == holeCard)
      return d.soul && d.blackHole ? "" : "Black Hole can't appear in this run";
    if (contains(cardPool[c.set], c.id)) return "";
    return key + " can't appear in a fresh run (it is locked, or needs something to "
                 "happen first, like a Planet's poker hand being played)";
  }
  case Condition::JOKER:
    for (const GameData::RarityPool &p : d.rarityPools) {
      const GameData::Entry *e = find(p.jokers, c.id);
      if (!e) continue;
      if (e->state == GameData::Entry::AVAILABLE) return "";
      if (e->state == GameData::Entry::GATED)
        return assumeEnhanced ? ""
                              : key + " only appears with an enhanced card in "
                                      "the deck: add 'assume enhanced'";
    }
    return key + " can't appear in the shops of a fresh run (it is locked, "
                 "or needs something to happen first)";
  case Condition::SOUL:
    if (!gd->soul) return "The Soul can't appear in this run";
    if (c.id != NONE && !contains(legendaryPool, c.id))
      return key + " can't come from The Soul in a fresh run";
    if (c.id != NONE && c.packs >= 2 && !assumeShowman)
      return key + " can't come from " + std::to_string(c.packs) +
             " Souls in one run without Showman: add 'assume showman'";
    return "";
  case Condition::BOSS: {
    size_t index = 0;
    for (size_t i = 0; i < d.bosses.size(); i++)
      if (d.bosses[i].id == c.id) index = i;
    for (int a = c.ante; a <= c.anteTo; a++)
      if (d.bossEligible(index, a)) return "";
    std::string antes;
    for (int a = 1; a <= MAX_ANTE; a++)
      if (d.bossEligible(index, a)) antes += (antes.empty() ? "" : ", ") + std::to_string(a);
    return key + " can't be the boss of " + anteWords(c) +
           (antes.empty() ? " (it is never the boss in this run)"
                          : " (only of ante " + antes + ")");
  }
  }
  return "";
}

std::unique_ptr<FilterProgram>
FilterProgram::parse(const std::string &text, std::string &error,
                     std::shared_ptr<const GameData> data) {
  std::unique_ptr<FilterProgram> prog(new FilterProgram());
  prog->gd = data ? data : GameData::vanilla();
  std::string normalised = text;
  std::replace(normalised.begin(), normalised.end(), ';', '\n');
  std::istringstream lines(normalised);
  std::string line;
  int lineNo = 0;
  std::vector<int> lineOf; // condition -> its line, for later errors
  while (std::getline(lines, line)) {
    lineNo++;
    std::istringstream in(line);
    std::vector<std::string> words;
    // Keys are percent-encoded like the game data's (gamedata.hpp).
    for (std::string w; in >> w;) words.push_back(GameData::decodeWord(w));
    if (words.empty() || words[0][0] == '#') continue;
    if (words[0] == "assume") {
      if (words.size() == 2 && words[1] == "enhanced") {
        prog->assumeEnhanced = true;
      } else if (words.size() == 2 && words[1] == "showman") {
        prog->assumeShowman = true;
      } else {
        error = "line " + std::to_string(lineNo) +
                ": expected 'assume enhanced' or 'assume showman'";
        return nullptr;
      }
      continue;
    }
    Condition c;
    std::string problem = parseLine(words, c, *prog->gd);
    if (!problem.empty()) {
      error = "line " + std::to_string(lineNo) + ": " + problem;
      return nullptr;
    }
    if (prog->all.size() >= 64) {
      error = "too many conditions (at most 64)";
      return nullptr;
    }
    prog->all.push_back(c);
    lineOf.push_back(lineNo);
  }

  // `assume ...` may be on any line, so the lists are worked out only now.
  prog->resolve();

  for (size_t i = 0; i < prog->all.size(); i++) {
    std::string problem = prog->impossible(prog->all[i]);
    if (!problem.empty()) {
      error = "line " + std::to_string(lineOf[i]) + ": " + problem;
      return nullptr;
    }
  }

  // Soul conditions on one ante (or on the same range) each take the next
  // Souls of it, in the order they are written: "soul ante=1
  // legendary=j_perkeo" then "soul ante=1 legendary=j_triboulet" asks for
  // Perkeo from the ante's first Soul and Triboulet from its second.
  {
    std::vector<std::pair<std::pair<int, int>, int>> rangeSouls; // range -> Souls
    for (Condition &c : prog->all) {
      if (c.kind != Condition::SOUL) continue;
      if (c.ante == c.anteTo) {
        c.first = prog->soulPacks[c.ante];
        prog->soulPacks[c.ante] += c.packs;
        continue;
      }
      std::pair<int, int> range(c.ante, c.anteTo);
      auto it = std::find_if(rangeSouls.begin(), rangeSouls.end(),
                             [&](const auto &r) { return r.first == range; });
      if (it == rangeSouls.end()) {
        rangeSouls.push_back({range, 0});
        it = rangeSouls.end() - 1;
      }
      c.first = it->second;
      it->second += c.packs;
    }
  }

  // Check order: cheapest kind first (the enum order is not that order).
  // Soul conditions go in ante order: each one's legendaries follow the
  // Souls of the antes before it.
  const Condition::Kind order[] = {Condition::BOSS, Condition::TAG,
                                   Condition::VOUCHER,
                                   Condition::PACK, Condition::SOUL,
                                   Condition::CARD, Condition::JOKER};
  for (Condition::Kind k : order)
    for (const Condition &c : prog->all)
      if (c.kind == k) prog->ordered.push_back(c);
  std::stable_sort(prog->ordered.begin(), prog->ordered.end(),
                   [](const Condition &a, const Condition &b) {
                     return a.kind == Condition::SOUL &&
                            b.kind == Condition::SOUL && a.ante < b.ante;
                   });

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
        if (c.buffoon) {
          if (prog->bufSize[a] != 0 && prog->bufSize[a] != c.size) {
            error = "Buffoon pack jokers in the same ante must use the same "
                    "pack size";
            return nullptr;
          }
          prog->bufSize[a] = c.size;
          prog->bufPacks[a] = std::max(prog->bufPacks[a], c.packs);
          for (size_t r = 0; r < prog->jokerPool.size(); r++)
            if (contains(prog->jokerPool[r], c.id)) prog->bufRarities[a][r] = 1;
          prog->bufEditions[a] |= !c.anyEdition;
          break;
        }
        for (size_t r = 0; r < prog->jokerPool.size(); r++)
          if (contains(prog->jokerPool[r], c.id)) prog->jokerRarities[a][r] = 1;
        prog->jokerEditions[a] |= !c.anyEdition;
        break;
      case Condition::CARD:
        if (c.from == Condition::SHOP) {
          prog->cardShop[a][c.set] = true;
          break;
        }
        // Cards of one kind of pack in one ante are read from the same packs.
        if (prog->cardPackSize[a][c.from] != 0 && prog->cardPackSize[a][c.from] != c.size) {
          error = "card conditions in the same ante must use the same pack size "
                  "for each kind of pack";
          return nullptr;
        }
        prog->cardPackSize[a][c.from] = c.size;
        break;
      case Condition::SOUL:
        // Soul conditions on one ante share its packs, which only works if
        // they open packs of the same size.
        if (prog->soulKind[a] != 0 && prog->soulKind[a] != (c.spectral ? 2 : 1)) {
          error = "soul conditions in the same ante must use the same kind of "
                  "pack (Arcana or Spectral)";
          return nullptr;
        }
        if (prog->soulSize[a] != 0 && prog->soulSize[a] != c.size) {
          error = "soul conditions in the same ante must use the same pack "
                  "size";
          return nullptr;
        }
        prog->soulSize[a] = c.size;
        prog->soulKind[a] = c.spectral ? 2 : 1;
        break;
      }
    }
  for (int a = 1; a <= MAX_ANTE; a++)
    prog->soulBase[a + 1] = prog->soulBase[a] + prog->soulPacks[a];
  // A soul condition and a card condition on the same kind of pack in one
  // ante read the same packs: same size.
  for (int a = 1; a <= MAX_ANTE; a++) {
    int f = prog->soulKind[a] == 1 ? Condition::ARCANA : prog->soulKind[a] == 2 ? Condition::SPECTRAL : -1;
    if (f >= 0 && prog->cardPackSize[a][f] != 0 && prog->cardPackSize[a][f] != prog->soulSize[a]) {
      error = std::string("soul and card conditions in the same ante must use the same size of ") +
              (f == Condition::ARCANA ? "Arcana" : "Spectral") + " pack";
      return nullptr;
    }
  }

  // Without Showman a legendary comes from The Soul once per run: two
  // single-ante soul conditions can't both get the same one.
  if (!prog->assumeShowman)
    for (size_t i = 0; i < prog->all.size(); i++)
      for (size_t j = i + 1; j < prog->all.size(); j++) {
        const Condition &a = prog->all[i], &b = prog->all[j];
        if (a.kind == Condition::SOUL && b.kind == Condition::SOUL &&
            a.id != NONE && a.id == b.id && a.ante == a.anteTo &&
            b.ante == b.anteTo) {
          error = "line " + std::to_string(lineOf[j]) + ": '" +
                  prog->gd->keyOf(a.id) +
                  "' can't come from The Soul twice in one run without "
                  "Showman (add 'assume showman')";
          return nullptr;
        }
      }

  return prog;
}

// ---------------------------------------------------------------------------
// The reference meaning: each condition on a fresh run, with Immolate's
// full generators. Simple on purpose; see filterprog.hpp. Unmodded game only.

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

// One condition at one ante (c.ante/c.anteTo only say whether it is a range).
bool FilterProgram::referenceAt(const Condition &cond, int ante,
                                Seed &seed) const {
  Condition c = cond;
  bool range = cond.ante != cond.anteTo;
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
  case Condition::CARD: {
    // Locks as the game marks cards on screen "used": a card can't come
    // twice in one shop or one pack, unless a Showman is held (and the
    // Soul / Black Hole rolls stop once one is in the pack).
    std::vector<Item> held;
    auto hold = [&](Item it) {
      if (!assumeShowman && it != Item::RETRY && !inst.isLocked(it)) {
        inst.lock(it);
        held.push_back(it);
      }
    };
    auto release = [&] {
      for (Item it : held) inst.unlock(it);
      held.clear();
    };
    if (c.from == Condition::SHOP) {
      for (int i = 0; i < c.within; i++) {
        if (i % gd->shopSlots == 0) release(); // a reroll: a new shop
        ShopItem it = inst.nextShopItem(c.ante);
        if (it.type != Item::T_Joker && it.item == c.item) return true;
        if (it.type != Item::T_Playing_Card) hold(it.item);
      }
      return false;
    }
    for (int p = 0; p < c.packs; p++) {
      bool found = false;
      for (int i = 0; i < c.size; i++) {
        Item it = c.from == Condition::ARCANA      ? inst.nextTarot(ItemSource::Arcana_Pack, c.ante, true)
                  : c.from == Condition::CELESTIAL ? inst.nextPlanet(ItemSource::Celestial_Pack, c.ante, true)
                                                   : inst.nextSpectral(ItemSource::Spectral_Pack, c.ante, true);
        found |= it == c.item;
        hold(it);
      }
      release();
      if (found) return true;
    }
    return false;
  }
  case Condition::JOKER: {
    if (c.buffoon) {
      // Instance::nextBuffoonPack, with the lock rule of the shop below.
      for (int p = 0; p < c.packs; p++) {
        std::vector<Item> inPack;
        bool found = false;
        for (int i = 0; i < c.size; i++) {
          JokerData j = inst.nextJoker(ItemSource::Buffoon_Pack, c.ante, true);
          found |= j.joker == c.item && (c.anyEdition || j.edition == c.edition);
          if (!assumeShowman && !inst.isLocked(j.joker)) {
            inst.lock(j.joker);
            inPack.push_back(j.joker);
          }
        }
        if (found) return true;
        for (Item j : inPack) inst.unlock(j);
      }
      return false;
    }
    // A joker in the shop can't be drawn again for the same shop (the game
    // counts it as used while it is on screen); a reroll clears the shop.
    // A Showman lifts that rule. (Not inst.params.showman: in Immolate that
    // also ignores locks Showman doesn't lift, such as the enhancement jokers.)
    std::vector<Item> inShop;
    for (int i = 0; i < c.within; i++) {
      if (i % gd->shopSlots == 0) {
        for (Item j : inShop) inst.unlock(j);
        inShop.clear();
      }
      ShopItem it = inst.nextShopItem(c.ante);
      if (it.item == c.item &&
          (c.anyEdition || it.jokerData.edition == c.edition))
        return true;
      if (!assumeShowman && it.type == Item::T_Joker &&
          !inst.isLocked(it.item)) {
        inst.lock(it.item);
        inShop.push_back(it.item);
      }
    }
    return false;
  }
  case Condition::SOUL: {
    inst.params.showman = assumeShowman;
    // The Souls used in earlier antes took the first legendaries. Each is
    // made in its own ante: the joker's edition comes from that ante's
    // stream ("edisou" + ante), so it must not use up a roll of this one's.
    if (!range)
      for (int b = 1; b < c.ante; b++)
        for (int k = 0; k < soulPacks[b]; k++) {
          Item j = inst.nextJoker(ItemSource::Soul, b, true).joker;
          if (!assumeShowman) inst.lock(j);
        }
    // The packs of the soul conditions written before this one on the same
    // ante come first, each with a Soul.
    for (int p = 0; p < c.first + c.packs; p++) {
      bool soul = false;
      std::vector<Item> pack = c.spectral ? inst.nextSpectralPack(c.size, c.ante)
                                          : inst.nextArcanaPack(c.size, c.ante);
      for (Item it : pack) soul |= it == Item::The_Soul;
      if (!soul) return false;
      JokerData made = inst.nextJoker(ItemSource::Soul, c.ante, true);
      if (!assumeShowman) inst.lock(made.joker);
      if (p < c.first) continue;
      if (c.item != Item::RETRY && made.joker != c.item) return false;
      if (!c.anyEdition && made.edition != c.edition) return false;
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
// The fast check. It reads only the filter's GameData lists; the Instance is
// used for its random streams alone. Each kind uses its own streams (tags:
// "Tag<ante>", packs: "shop_pack<ante>", ...), so the kinds can be checked
// in any order. What a seed offers in an ante is worked out on first use and
// kept (SeedFacts); state that carries over between antes (bought vouchers,
// bosses seen, legendaries given) is built up in ante order.

namespace {

// The game's pick from a list: item number N, and if that slot is
// unavailable, again with "<stream>_resample2", "_resample3", ... (the loop
// in get_next_tag_key, get_next_voucher_key and create_card). `taken` says
// which other items are unavailable right now. If the game removes
// unavailable items from its lists instead (pad == false), there is nothing
// to re-roll: the list is just shorter.
template <class Taken>
int pick(Instance &inst, const FilterProgram::StreamKeys &keys, double *node,
         const std::vector<int> &items, bool pad, Taken taken) {
  if (!pad) {
    thread_local std::vector<int> left;
    left.clear();
    for (int it : items)
      if (it != NONE && !taken(it)) left.push_back(it);
    if (left.empty()) return NONE;
    return left[luaRandint(inst.step_node(node), 0, (int)left.size() - 1)];
  }
  int n = (int)items.size();
  if (n == 0) return NONE;
  int it = items[luaRandint(inst.step_node(node), 0, n - 1)];
  for (int r = 2; it == NONE || taken(it); r++) {
    if (r > 1000) return NONE; // nothing available at all
    double seed = r <= 9 ? inst.get_node(keys.re[r - 2])
                         : inst.get_node(keys.base + "_resample" + std::to_string(r));
    it = items[luaRandint(seed, 0, n - 1)];
  }
  return it;
}

bool never(int) { return false; }

// ---------------------------------------------------------------------------
// Weighted games. Steamodded's SMODS.poll_object (src/utils/weights.lua)
// gives every item of a list a running total of the weights up to it, turns
// the roll x into a number between 0 and the total, and looks up whose
// stretch it falls in with a binary search. The arithmetic and the search
// below are the same, step for step, so the result is too (the build
// forbids the compiler to merge or reorder floating-point operations).

// SMODS.select_by_weight, with the "first item" shortcut poll_object and
// poll_object_type make before calling it. cum: running totals, n > 0.
int selectByWeight(const double *cum, int n, double poll) {
  if (!(poll > cum[0])) return 0;
  int low = 1, high = n; // counted from 1, as in the Lua code
  while (high - low > 1) {
    int check = (low + high) / 2;
    if (poll < cum[check - 1]) high = check;
    else low = check;
  }
  return high - 1;
}

// One pick from a weighted list. A slot without an item (id NONE) makes the
// game roll again on the same stream.
int drawWeighted(Instance &inst, double *node, const int *id, const double *cum,
                 int n) {
  if (n == 0) return NONE;
  double total = cum[n - 1];
  for (int tries = 0; tries < 1000; tries++) {
    // total - x * total falls as x rises, so the index never rises: fine
    // for luaRandomDecide.
    int i = luaRandomDecide(inst.step_node(node), [&](double x) {
      return selectByWeight(cum, n, total - x * total);
    });
    if (id[i] != NONE) return id[i];
  }
  return NONE;
}

// The same from one of the lists that change with what is on offer:
// `present` are the items in the shop (or already given by The Soul). An
// item that is present can't come again, and the game data's links say
// which other weights change (gamedata.hpp).
int drawWeighted(Instance &inst, double *node, const FilterProgram::WPool &p,
                 const GameData &d, const int *present, int numPresent) {
  bool changed = false;
  for (int k = 0; k < numPresent; k++)
    changed |= present[k] != NONE && p.affected[present[k]];
  if (!changed)
    return drawWeighted(inst, node, p.id.data(), p.cum.data(), (int)p.id.size());
  thread_local std::vector<double> w;
  w = p.w;
  for (int k = 0; k < numPresent; k++)
    if (const std::vector<GameData::Link> *links = d.linksOf(present[k]))
      for (const GameData::Link &l : *links)
        if (p.indexOf[l.id] >= 0) w[p.indexOf[l.id]] = l.weight;
  for (int k = 0; k < numPresent; k++)
    if (present[k] != NONE && p.indexOf[present[k]] >= 0) w[p.indexOf[present[k]]] = 0;
  double total = 0;
  for (double &x : w) x = total += x; // weights -> running totals
  return drawWeighted(inst, node, p.id.data(), w.data(), (int)w.size());
}

} // namespace

// Everything the fast check has learned about one seed so far. One per
// thread, reused from seed to seed so no memory is allocated per seed.
struct SeedFacts {
  const FilterProgram *prog = nullptr;
  const GameData *d = nullptr;
  Instance *inst = nullptr;
  static const int A = FilterProgram::MAX_ANTE + 1;

  struct Card {
    // NONE: not a joker, or of a rarity nobody asked about. In a weighted
    // game: whatever the card is (joker or consumable), NONE for a playing
    // card.
    int joker = NONE;
    int edition = NONE; // NONE: no edition
  };

  // Weighted games: the blinds, vouchers and tags of antes 1..startDone are
  // drawn; typeNode[a] is the stream they share with ante a's shop packs.
  int startDone = 0;
  double *typeNode[A];
  double *soulDrawNode[A];
  // What the Souls of one ante give when no Soul came before (for
  // conditions on a range of antes), each ante on its own copy of the stream.
  std::vector<int> rangeLegends[A];
  double rangeNode[A];

  bool tagsDone = false;
  int small[A], big[A];

  bool vouchersDone = false;
  int voucher[A];
  std::vector<int> voucherList;   // the voucher list as it is right now
  std::vector<char> voucherBought;

  bool bossesDone = false;
  int boss[A];
  std::vector<unsigned char> bossUsed;
  std::vector<int> bossPick;

  std::vector<int> packs[A];
  double *packNode[A];

  int souls[A];       // packs opened so far in that ante, each with a Soul
  bool soulsEnded[A]; // a pack without a Soul was opened: no more count
  double *soulNode[A];
  double *soulEditionNode[A];
  std::vector<int> soulEditions[A];
  std::vector<int> legends; // what the 1st, 2nd, ... Soul of the run gives

  // One ante's shop cards, in the order the game makes them.
  struct Shop {
    bool started = false;
    double *cdt, *rarity, *edition;
    double *stream = nullptr;   // weighted games: the stream all cards share
    std::vector<double *> pool; // per rarity pool, null until first used
    double *cards[GameData::CARD_SETS]; // consumable lists, null until first used
    int slot = 0;               // cards made for the current shop
    int inShop[8], numInShop = 0;
    std::vector<Card> list;
  };
  Shop shop[A];

  // Jokers in Buffoon packs, per ante: like the shop, but every card is a
  // joker and the "no joker twice" rule is per pack.
  struct Buffoon {
    bool started = false;
    double *rarity, *edition;
    std::vector<double *> pool;
    double *stream; // weighted games: the one stream all its jokers come from
    std::vector<Card> list; // pack after pack, bufSize[a] cards each
  };
  Buffoon buf[A];

  // Consumable packs, per ante and kind of pack (Condition::From): the
  // cards of each pack opened so far, The Soul / Black Hole as soulCard /
  // holeCard. The Soul / Black Hole rolls use a copy of their stream of
  // their own ("soul_Tarot" + ante is also read by needSouls).
  struct CardPacks {
    bool started = false;
    double roll, modRoll;
    double *pool;
    std::vector<std::vector<int>> packs;
  };
  CardPacks cardPacks[A][4];

  void reset(const FilterProgram *p, Instance *i) {
    prog = p, inst = i, d = p ? &p->data() : nullptr;
    tagsDone = vouchersDone = bossesDone = false;
    startDone = 0;
    legends.clear();
    for (int a = 0; a < A; a++) {
      packs[a].clear();
      packNode[a] = soulNode[a] = soulEditionNode[a] = nullptr;
      soulEditions[a].clear();
      typeNode[a] = soulDrawNode[a] = nullptr;
      rangeLegends[a].clear();
      souls[a] = 0;
      soulsEnded[a] = false;
      shop[a].started = false;
      shop[a].list.clear();
      buf[a].started = false;
      buf[a].list.clear();
      for (int f = 0; f < 4; f++) {
        cardPacks[a][f].started = false;
        cardPacks[a][f].packs.clear();
      }
    }
  }

  // Weighted games: everything an ante starts with, in the order the game
  // draws it from the ante's shared stream. Ante 1 (Game:start_run): the
  // three blinds, the voucher, the two tags. Later antes: the voucher when
  // the boss is beaten (end_round), then the tags and the three blinds on
  // cashing out (G.FUNCS.cash_out, reset_blinds). Antes are done in order:
  // a voucher depends on the ones bought before, a boss on the ones seen.
  void needStart(int a) {
    for (int b = startDone + 1; b <= a; b++) {
      startDone = b;
      typeNode[b] = inst->node_ref(prog->typeKey[b]);
      if (b == 1) {
        size_t n = d->vouchers.size();
        voucherList.resize(n);
        voucherBought.assign(n, 0);
        for (size_t i = 0; i < n; i++)
          voucherList[i] = d->vouchers[i].start ? d->vouchers[i].id : NONE;
        bossUsed.assign(d->bosses.size(), 0);
        startBlinds(b);
        startVouchers(b);
        startTags(b);
      } else {
        startVouchers(b);
        startTags(b);
        startBlinds(b);
      }
    }
  }

  void startTags(int b) {
    const FilterProgram::WPool &p = prog->wTags[b];
    small[b] = drawWeighted(*inst, typeNode[b], p.id.data(), p.cum.data(), (int)p.id.size());
    big[b] = drawWeighted(*inst, typeNode[b], p.id.data(), p.cum.data(), (int)p.id.size());
  }

  // The Small and Big Blind are "picked" too (one roll each, from lists of
  // one), then the boss: of the bosses allowed on the ante, those seen least
  // often, in key order, by weight.
  void startBlinds(int b) {
    inst->step_node(typeNode[b]);
    inst->step_node(typeNode[b]);
    thread_local std::vector<int> ids, index;
    thread_local std::vector<double> cum;
    ids.clear(), index.clear(), cum.clear();
    size_t n = d->bosses.size();
    int least = 255;
    for (size_t i = 0; i < n; i++)
      if (d->bossEligible(i, b) && bossUsed[i] < least) least = bossUsed[i];
    double total = 0;
    for (size_t i = 0; i < n; i++)
      if (d->bossEligible(i, b) && bossUsed[i] == least) {
        ids.push_back((int)i); // the position: ids of two bosses never clash
        total += d->bosses[i].weight;
        cum.push_back(total);
      }
    if (ids.empty()) { // the game falls back to a default boss, with a roll
      inst->step_node(typeNode[b]);
      boss[b] = NONE;
      return;
    }
    int chosen = drawWeighted(*inst, typeNode[b], ids.data(), cum.data(), (int)ids.size());
    bossUsed[chosen]++;
    boss[b] = d->bosses[chosen].id;
  }

  // The ante's vouchers (usually one), from the vouchers available now. A
  // voucher already picked for this shop stays in the list as a slot that
  // is rolled again (SMODS.get_next_vouchers). The first one counts as
  // bought before the next ante, as in an unweighted game.
  void startVouchers(int b) {
    thread_local std::vector<int> ids;
    thread_local std::vector<double> weights, cum;
    ids.clear(), weights.clear();
    for (size_t i = 0; i < d->vouchers.size(); i++)
      if (voucherList[i] != NONE) {
        ids.push_back((int)i);
        weights.push_back(d->vouchers[i].weight);
      }
    voucher[b] = NONE;
    if (ids.empty()) { // the game falls back to a default voucher, with a roll
      inst->step_node(typeNode[b]);
      return;
    }
    int draws = std::min((int)ids.size(), d->vouchersPerAnte);
    for (int k = 0; k < draws; k++) {
      cum.resize(ids.size());
      double total = 0;
      for (size_t i = 0; i < ids.size(); i++) cum[i] = total += weights[i];
      int chosen = drawWeighted(*inst, typeNode[b], ids.data(), cum.data(), (int)ids.size());
      if (chosen == NONE) break;
      if (k == 0) voucher[b] = d->vouchers[chosen].id;
      for (size_t i = 0; i < ids.size(); i++)
        if (ids[i] == chosen) ids[i] = NONE, weights[i] = GameData::DEFAULT_WEIGHT;
    }
    if (voucher[b] == NONE) return;
    int bought = prog->voucherIndex[voucher[b]];
    voucherBought[bought] = 1;
    voucherList[bought] = NONE;
    for (int next : prog->voucherUnlocks[bought]) {
      const GameData::Voucher &u = d->vouchers[next];
      bool ready = u.later && !voucherBought[next];
      for (int need : u.needs)
        ready = ready && prog->voucherIndex[need] >= 0 &&
                voucherBought[prog->voucherIndex[need]];
      if (ready) voucherList[next] = u.id;
    }
  }

  void needTags() {
    if (tagsDone) return;
    tagsDone = true;
    for (int a = 1; a <= prog->maxTagAnte; a++) {
      if (!prog->tagAnte[a]) continue;
      double *node = inst->node_ref(prog->tagKeys[a].base);
      small[a] = pick(*inst, prog->tagKeys[a], node, prog->tagPool[a], d->pad, never);
      big[a] = pick(*inst, prog->tagKeys[a], node, prog->tagPool[a], d->pad, never);
    }
  }

  // The voucher of each ante, with every earlier one bought: a bought
  // voucher leaves the list, and the vouchers that needed it join.
  void needVouchers() {
    if (vouchersDone) return;
    vouchersDone = true;
    size_t n = d->vouchers.size();
    voucherList.resize(n);
    voucherBought.assign(n, 0);
    for (size_t i = 0; i < n; i++)
      voucherList[i] = d->vouchers[i].start ? d->vouchers[i].id : NONE;
    for (int a = 1; a <= prog->maxVoucherAnte; a++) {
      double *node = inst->node_ref(prog->voucherKeys[a].base);
      int v = pick(*inst, prog->voucherKeys[a], node, voucherList, d->pad, never);
      voucher[a] = v;
      if (v == NONE) continue;
      int bought = prog->voucherIndex[v];
      voucherBought[bought] = 1;
      voucherList[bought] = NONE;
      for (int next : prog->voucherUnlocks[bought]) {
        const GameData::Voucher &u = d->vouchers[next];
        bool ready = u.later && !voucherBought[next];
        for (int need : u.needs)
          ready = ready && prog->voucherIndex[need] >= 0 &&
                  voucherBought[prog->voucherIndex[need]];
        if (ready) voucherList[next] = u.id;
      }
    }
  }

  // get_new_boss(): of the bosses allowed on the ante, one of those seen
  // least often so far, in key order.
  void needBosses() {
    if (bossesDone) return;
    bossesDone = true;
    size_t n = d->bosses.size();
    bossUsed.assign(n, 0);
    bossPick.resize(n);
    double *node = inst->node_ref("boss");
    for (int a = 1; a <= prog->maxBossAnte; a++) {
      int least = 255, count = 0;
      for (size_t i = 0; i < n; i++)
        if (d->bossEligible(i, a) && bossUsed[i] < least) least = bossUsed[i];
      for (size_t i = 0; i < n; i++)
        if (d->bossEligible(i, a) && bossUsed[i] == least) bossPick[count++] = (int)i;
      if (count == 0) {
        boss[a] = NONE;
        continue;
      }
      int chosen = bossPick[luaRandint(inst->step_node(node), 0, count - 1)];
      bossUsed[chosen]++;
      boss[a] = d->bosses[chosen].id;
    }
  }

  // The first n packs offered in ante a.
  const std::vector<int> &needPacks(int a, int n) {
    std::vector<int> &list = packs[a];
    // Ante 1's first shop opens with a fixed pack, without a random draw.
    if (list.empty() && a == 1 && d->firstPack != NONE) list.push_back(d->firstPack);
    if (d->weighted) {
      // After everything the ante starts with, on the same stream.
      needStart(a);
      const FilterProgram::WPool &p = prog->wPacks;
      while ((int)list.size() < n)
        list.push_back(drawWeighted(*inst, typeNode[a], p.id.data(), p.cum.data(),
                                    (int)p.id.size()));
      return list;
    }
    if ((int)list.size() < n && !packNode[a]) packNode[a] = inst->node_ref(prog->packKey[a]);
    while ((int)list.size() < n) {
      if (d->packs.value.empty()) {
        list.push_back(NONE);
        continue;
      }
      int step = luaRandomDecide(inst->step_node(packNode[a]),
                                 [&](double x) { return d->packs.index(x); });
      list.push_back(d->packs.value[step]);
    }
    return list;
  }

  // How many packs in a row (up to n) contain The Soul in ante a. Each card
  // rolls for it, until the pack has one: a Soul on screen stops more from
  // appearing, unless a Showman is held.
  int needSouls(int a, int n) {
    if (prog->soulKind[a] == 2) return needSpectralSouls(a, n);
    while (souls[a] < n && !soulsEnded[a]) {
      bool found = false;
      if (d->soul) {
        if (!soulNode[a]) soulNode[a] = inst->node_ref(prog->soulKey[a]);
        for (int i = 0; i < prog->soulSize[a]; i++) {
          bool hit = luaRandomDecide(inst->step_node(soulNode[a]),
                                     [](double x) { return x > 0.997; });
          found |= hit;
          if (hit && !prog->assumeShowman) break;
        }
      }
      if (found) souls[a]++;
      else soulsEnded[a] = true;
    }
    return souls[a];
  }

  // The same for Spectral packs (create_card in the game's
  // functions/common_events.lua). Every Spectral card rolls twice on one
  // stream, "soul_Spectral" + ante: first for The Soul, then for a Black
  // Hole, which wins if both come up. A Soul or Black Hole already in the
  // pack skips its roll for the cards after it (unless a Showman is held).
  // Unlike Arcana packs, finding a Soul doesn't end a pack early: the cards
  // after it still roll for a Black Hole, and those rolls move the stream
  // on for the next pack.
  int needSpectralSouls(int a, int n) {
    auto roll = [&] {
      return luaRandomDecide(inst->step_node(soulNode[a]), [](double x) { return x > 0.997; });
    };
    while (souls[a] < n && !soulsEnded[a]) {
      bool found = false;
      if (d->soul) {
        if (!soulNode[a]) soulNode[a] = inst->node_ref(prog->soulSpectralKey[a]);
        bool soulInPack = false, holeInPack = false;
        bool showman = prog->assumeShowman;
        for (int i = 0; i < prog->soulSize[a]; i++) {
          bool soul = (showman || !soulInPack) && roll();
          bool hole = d->blackHole && (showman || !holeInPack) && roll();
          if (hole) holeInPack = true;
          else if (soul) soulInPack = found = true;
        }
      }
      if (found) souls[a]++;
      else soulsEnded[a] = true;
    }
    return souls[a];
  }

  // What the k-th Soul of the run gives (k from 1).
  int legend(int k) {
    while (d->weighted && (int)legends.size() < k) {
      // The stream belongs to the ante the Soul is used in: Soul number i
      // of the run is opened in the first ante b with soulBase[b + 1] >= i.
      int i = (int)legends.size() + 1, b = 1;
      while (b < FilterProgram::MAX_ANTE && prog->soulBase[b + 1] < i) b++;
      if (!soulDrawNode[b]) soulDrawNode[b] = inst->node_ref(prog->soulDrawKey[b]);
      bool showman = prog->assumeShowman;
      legends.push_back(drawWeighted(*inst, soulDrawNode[b], prog->wLegendary, *d,
                                     legends.data(), showman ? 0 : (int)legends.size()));
    }
    while ((int)legends.size() < k) {
      double *node = inst->node_ref(prog->legendaryKeys.base);
      bool showman = prog->assumeShowman;
      legends.push_back(pick(*inst, prog->legendaryKeys, node, prog->legendaryPool,
                             d->pad, [&](int id) {
                               return !showman && std::find(legends.begin(), legends.end(),
                                                            id) != legends.end();
                             }));
    }
    return legends[k - 1];
  }

  // Weighted games: what the k-th Soul opened in ante a gives when no Soul
  // was used before that ante (k from 1).
  int legendInAnte(int a, int k) {
    std::vector<int> &list = rangeLegends[a];
    if (list.empty()) {
      const std::string &key = prog->soulDrawKey[a];
      rangeNode[a] = pseudohash_from(key, inst->seed.pseudohash(key.length()));
    }
    bool showman = prog->assumeShowman;
    while ((int)list.size() < k)
      list.push_back(drawWeighted(*inst, &rangeNode[a], prog->wLegendary, *d,
                                  list.data(), showman ? 0 : (int)list.size()));
    return list[k - 1];
  }

  // The edition of the j-th Soul joker made in ante a (j from 1), as a
  // number in the game data; NONE = no edition.
  int soulEdition(int a, int j) {
    std::vector<int> &list = soulEditions[a];
    // Look the stream up the first time this ante is asked about.
    if (!soulEditionNode[a]) soulEditionNode[a] = inst->node_ref(prog->soulEditionKey[a]);
    while ((int)list.size() < j) {
      int e = luaRandomDecide(inst->step_node(soulEditionNode[a]),
                              [&](double x) { return d->editions.index(x); });
      list.push_back(d->editions.value[e]);
    }
    return list[j - 1];
  }

  // Weighted games: the next shop card of ante a (SMODS.create_shop_card).
  // The kind of card comes from the "cdt" stream as before, by rate; then
  // every kind picks its item from the one shop stream. A joker rolls its
  // rarity and edition on their own streams.
  Card nextWeightedShopCard(int a) {
    Shop &s = shop[a];
    Card card;
    if (s.slot == d->shopSlots) s.slot = 0, s.numInShop = 0; // a new shop
    s.slot++;
    int kinds = (int)d->shopTypes.size();
    if (kinds == 0) return card;
    const double *cum = prog->wTypeCum.data();
    double total = cum[kinds - 1];
    int t = luaRandomDecide(inst->step_node(s.cdt), [&](double x) {
      return selectByWeight(cum, kinds, x * total);
    });
    const FilterProgram::WPool *pool = &prog->wTypes[t];
    if (d->shopTypes[t].joker) {
      if (d->rarities.value.empty()) return card;
      int step = 0;
      for (int p = 0; p < d->rarityPolls; p++)
        step = luaRandomDecide(inst->step_node(s.rarity),
                               [&](double x) { return d->rarities.index(x); });
      pool = &prog->wRarity[d->rarities.value[step]];
      if (prog->jokerEditions[a]) {
        int e = luaRandomDecide(inst->step_node(s.edition),
                                [&](double x) { return d->editions.index(x); });
        card.edition = d->editions.value[e];
      }
    }
    if (pool->id.empty()) return card; // a kind that draws nothing
    // With a Showman nothing on offer is excluded.
    card.joker = drawWeighted(*inst, s.stream, *pool, *d, s.inShop,
                              prog->assumeShowman ? 0 : s.numInShop);
    if (card.joker != NONE && s.numInShop < 8) s.inShop[s.numInShop++] = card.joker;
    return card;
  }

  // The next shop card of ante a: create_card_for_shop, then create_card
  // for a joker (rarity, edition, then which joker of that rarity).
  Card nextShopCard(int a) {
    Shop &s = shop[a];
    Card none;
    if (s.slot == d->shopSlots) s.slot = 0, s.numInShop = 0; // a new shop
    s.slot++;
    // All the tests below never go down as the roll goes up, so
    // luaRandomDecide gives the full computation's answer (util.hpp).
    // The kind of card, in create_card_for_shop's order: 0 joker, 1 Tarot,
    // 2 Planet, 3 playing card, 4 Spectral, 5 something else (a mod's).
    double total = d->shopTotal;
    const double upTo[5] = {d->shopJoker, d->shopJoker + d->shopTarot,
                            d->shopJoker + d->shopTarot + d->shopPlanet,
                            d->shopJoker + d->shopTarot + d->shopPlanet + d->shopPlaying,
                            d->shopJoker + d->shopTarot + d->shopPlanet + d->shopPlaying +
                                d->shopSpectral};
    int type = luaRandomDecide(inst->step_node(s.cdt), [&](double x) {
      double r = x * total;
      int t = 0;
      while (t < 5 && r >= upTo[t]) t++;
      return t;
    });
    if (type == 1 || type == 2 || type == 4) {
      int set = type == 1 ? 0 : type == 2 ? 1 : 2;
      if (!prog->cardShop[a][set]) return none; // nobody asked: stream skipped
      if (!s.cards[set]) s.cards[set] = inst->node_ref(prog->cardShopKeys[a][set].base);
      Card card;
      card.joker = pick(*inst, prog->cardShopKeys[a][set], s.cards[set], prog->cardPool[set],
                        d->pad, [&](int id) {
                          if (prog->assumeShowman) return false;
                          for (int i = 0; i < s.numInShop; i++)
                            if (s.inShop[i] == id) return true;
                          return false;
                        });
      if (card.joker != NONE && s.numInShop < 8) s.inShop[s.numInShop++] = card.joker;
      return card;
    }
    if (type != 0 || d->rarities.value.empty()) return none;
    int step = 0;
    for (int p = 0; p < d->rarityPolls; p++)
      step = luaRandomDecide(inst->step_node(s.rarity),
                             [&](double x) { return d->rarities.index(x); });
    int r = d->rarities.value[step];
    Card card;
    // Every joker rolls for an edition, whatever its rarity, so the edition
    // stream stays in step.
    if (prog->jokerEditions[a]) {
      int e = luaRandomDecide(inst->step_node(s.edition),
                              [&](double x) { return d->editions.index(x); });
      card.edition = d->editions.value[e];
    }
    if (!prog->jokerRarities[a][r]) return card; // nobody asked: stream skipped
    if (!s.pool[r]) s.pool[r] = inst->node_ref(prog->jokerKeys[a][r].base);
    card.joker = pick(*inst, prog->jokerKeys[a][r], s.pool[r], prog->jokerPool[r],
                      d->pad, [&](int id) {
                        // No joker twice in one shop, unless a Showman is held.
                        if (prog->assumeShowman) return false;
                        for (int i = 0; i < s.numInShop; i++)
                          if (s.inShop[i] == id) return true;
                        return false;
                      });
    if (card.joker != NONE && s.numInShop < 8) s.inShop[s.numInShop++] = card.joker;
    return card;
  }

  // The next Buffoon pack joker of ante a (create_card with 'buf').
  Card nextBuffoonCard(int a) {
    Buffoon &b = buf[a];
    Card card;
    if (d->rarities.value.empty()) return card;
    int size = prog->bufSize[a];
    size_t start = b.list.size() - b.list.size() % size; // this pack's first
    int step = 0;
    for (int p = 0; p < d->rarityPolls; p++)
      step = luaRandomDecide(inst->step_node(b.rarity),
                             [&](double x) { return d->rarities.index(x); });
    int r = d->rarities.value[step];
    if (prog->bufEditions[a]) {
      int e = luaRandomDecide(inst->step_node(b.edition),
                              [&](double x) { return d->editions.index(x); });
      card.edition = d->editions.value[e];
    }
    if (d->weighted) {
      // poll_object, as for a shop joker, on the pack's own stream: one
      // stream for every rarity, so it is always drawn. A joker already in
      // the pack can't come again (unless a Showman is held), and its links
      // change the others' weights.
      int present[8], n = 0;
      if (!prog->assumeShowman)
        for (size_t i = start; i < b.list.size() && n < 8; i++)
          if (b.list[i].joker != NONE) present[n++] = b.list[i].joker;
      card.joker = drawWeighted(*inst, b.stream, prog->wRarity[r], *d, present, n);
      return card;
    }
    if (!prog->bufRarities[a][r]) return card; // nobody asked: stream skipped
    if (!b.pool[r]) b.pool[r] = inst->node_ref(prog->bufJokerKeys[a][r].base);
    card.joker = pick(*inst, prog->bufJokerKeys[a][r], b.pool[r], prog->jokerPool[r],
                      d->pad, [&](int id) {
                        if (prog->assumeShowman) return false;
                        for (size_t i = start; i < b.list.size(); i++)
                          if (b.list[i].joker == id) return true;
                        return false;
                      });
    return card;
  }

  // The first n packs of a kind (Condition::From) opened in ante a, each of
  // prog->cardPackSize[a][from] cards (create_card with soulable = true):
  // a card first rolls for The Soul (Arcana, Spectral) and Black Hole
  // (Celestial, Spectral; Black Hole wins), each only while none is in the
  // pack yet (unless a Showman is held), as needSouls does; if that misses,
  // it draws from the list, skipping the cards already in the pack.
  const std::vector<std::vector<int>> &needCardPacks(int a, int from, int n) {
    CardPacks &p = cardPacks[a][from];
    if (!p.started) {
      p.started = true;
      const std::string &key = prog->cardRollKey[a][from];
      p.roll = pseudohash_from(key, inst->seed.pseudohash(key.length()));
      const std::string &mkey = prog->cardModRollKey[a][from];
      p.modRoll = pseudohash_from(mkey, inst->seed.pseudohash(mkey.length()));
      p.pool = inst->node_ref(d->weighted ? prog->cardPackStream[a][from]
                                          : prog->cardPackKeys[a][from].base);
    }
    int size = prog->cardPackSize[a][from], set = from - 1;
    bool showman = prog->assumeShowman;
    auto roll = [&] {
      return luaRandomDecide(inst->step_node(&p.roll), [](double x) { return x > 0.997; });
    };
    while ((int)p.packs.size() < n) {
      std::vector<int> pack;
      bool soulIn = false, holeIn = false;
      for (int i = 0; i < size; i++) {
        int got = NONE;
        bool forced = false;
        if (d->soul) {
          // A mod's Soul-like cards first (Steamodded's create_card): one
          // roll per card on its own stream; each card of the set has a
          // share of the threshold. One already in the pack is out, unless
          // it may repeat or a Showman is held.
          // (The roll is made even when no such card exists.)
          const std::vector<GameData::ModSoul> &mods = prog->modSouls[from];
          {
            double total = 0, non = 1, thr[16];
            int ids[16], k = 0;
            for (const GameData::ModSoul &m : mods) {
              if (k == 16) break;
              if (!showman && !m.repeat && std::find(pack.begin(), pack.end(), m.id) != pack.end())
                continue;
              total += m.rate;
              non = std::max(non * (1 - m.rate), 0.0);
              ids[k++] = m.id;
            }
            double t = 1;
            for (int j = 0, q = 0; j < (int)mods.size() && q < k; j++) {
              if (mods[j].id != ids[q]) continue;
              t -= mods[j].rate / total * (1 - non);
              thr[q++] = t;
            }
            // How many thresholds the roll is above (they go down): the
            // first one passed is the card, as the game's loop finds it.
            int above = luaRandomDecide(inst->step_node(&p.modRoll), [&](double x) {
              int n = 0;
              for (int q = 0; q < k; q++) n += x > thr[q];
              return n;
            });
            if (above > 0) got = ids[k - above], forced = true;
          }
          bool soul = from != Condition::CELESTIAL && (showman || !soulIn) && roll();
          bool hole = from != Condition::ARCANA && d->blackHole && (showman || !holeIn) && roll();
          if (hole) got = prog->holeCard, holeIn = forced = true;
          else if (soul) got = prog->soulCard, soulIn = forced = true;
        }
        if (!forced && d->weighted)
          // poll_object: by weight; a card in the pack can't come again.
          got = drawWeighted(*inst, p.pool, prog->wCards[set], *d, pack.data(),
                             showman ? 0 : (int)pack.size());
        else if (!forced)
          got = pick(*inst, prog->cardPackKeys[a][from], p.pool, prog->cardPool[set], d->pad,
                     [&](int id) {
                       return !showman && std::find(pack.begin(), pack.end(), id) != pack.end();
                     });
        pack.push_back(got);
      }
      p.packs.push_back(std::move(pack));
    }
    return p.packs;
  }

  // The first n Buffoon pack jokers of ante a.
  const std::vector<Card> &needBuffoon(int a, int n) {
    Buffoon &b = buf[a];
    if (!b.started) {
      b.started = true;
      b.rarity = inst->node_ref(prog->bufRarityKey[a]);
      b.edition = prog->bufEditions[a] ? inst->node_ref(prog->bufEditionKey[a]) : nullptr;
      b.pool.assign(d->rarityPools.size(), nullptr);
      b.stream = d->weighted ? inst->node_ref(prog->bufStream[a]) : nullptr;
    }
    while ((int)b.list.size() < n) b.list.push_back(nextBuffoonCard(a));
    return b.list;
  }

  // The first n shop cards of ante a.
  const std::vector<Card> &needShop(int a, int n) {
    Shop &s = shop[a];
    if (!s.started) {
      s.started = true;
      s.slot = s.numInShop = 0;
      s.cdt = inst->node_ref(prog->cdtKey[a]);
      s.rarity = inst->node_ref(prog->rarityKey[a]);
      s.edition = prog->jokerEditions[a] ? inst->node_ref(prog->editionKey[a]) : nullptr;
      s.pool.assign(d->rarityPools.size(), nullptr);
      for (double *&n : s.cards) n = nullptr;
      s.stream = d->weighted ? inst->node_ref(prog->shopKey[a]) : nullptr;
    }
    while ((int)s.list.size() < n)
      s.list.push_back(d->weighted ? nextWeightedShopCard(a) : nextShopCard(a));
    return s.list;
  }

  bool holdsAt(const Condition &c, int a) {
    switch (c.kind) {
    case Condition::TAG:
      if (d->weighted) needStart(a);
      else needTags();
      return (c.blind != 2 && small[a] == c.id) ||
             (c.blind != 1 && big[a] == c.id);
    case Condition::VOUCHER:
      if (d->weighted) needStart(a);
      else needVouchers();
      return voucher[a] == c.id;
    case Condition::BOSS:
      if (d->weighted) needStart(a);
      else needBosses();
      return boss[a] == c.id;
    case Condition::PACK: {
      const std::vector<int> &list = needPacks(a, c.within);
      return std::find(list.begin(), list.begin() + c.within, c.id) !=
             list.begin() + c.within;
    }
    case Condition::SOUL: {
      int last = c.first + c.packs;
      if (needSouls(a, last) < last) return false;
      if (c.id == NONE && c.anyEdition) return true;
      // Single antes: after the Souls of the antes before (those conditions
      // were checked first, and held). Ranges: the run's first Souls.
      bool single = c.ante == c.anteTo;
      int before = single ? prog->soulBase[a] : 0;
      for (int p = c.first + 1; p <= last; p++) {
        int got = d->weighted && !single ? legendInAnte(a, p) : legend(before + p);
        if ((got != c.id && c.id != NONE) || (!c.anyEdition && soulEdition(a, p) != c.editionId)) return false;
      }
      return true;
    }
    case Condition::CARD: {
      if (c.from == Condition::SHOP) {
        const std::vector<Card> &list = needShop(a, c.within);
        for (int i = 0; i < c.within; i++)
          if (list[i].joker == c.id) return true;
        return false;
      }
      const std::vector<std::vector<int>> &packs = needCardPacks(a, c.from, c.packs);
      for (int p = 0; p < c.packs; p++)
        if (std::find(packs[p].begin(), packs[p].end(), c.id) != packs[p].end()) return true;
      return false;
    }
    case Condition::JOKER: {
      int n = c.buffoon ? c.packs * c.size : c.within;
      const std::vector<Card> &list = c.buffoon ? needBuffoon(a, n) : needShop(a, n);
      for (int i = 0; i < n; i++)
        if (list[i].joker == c.id &&
            (c.anyEdition || list[i].edition == c.editionId))
          return true;
      return false;
    }
    }
    return false;
  }
};

bool FilterProgram::matches(Instance &inst) const {
  thread_local SeedFacts facts;
  facts.reset(this, &inst);
  bool all = true;
  for (const Condition &c : ordered) {
    bool holds = false;
    for (int a = c.ante; a <= c.anteTo && !holds; a++) holds = facts.holdsAt(c, a);
    if (!holds) {
      all = false;
      break;
    }
  }
  facts.reset(nullptr, nullptr); // drop the pointers into `inst`
  return all;
}

bool FilterProgram::matches(Seed &seed) const {
  Instance inst(seed);
  return matches(inst);
}

FilterProgram::Start FilterProgram::predictStart(std::shared_ptr<const GameData> data,
                                                 Seed &seed) {
  FilterProgram prog;
  prog.gd = data ? data : GameData::vanilla();
  prog.resolve();
  prog.tagAnte[1] = true;
  prog.maxTagAnte = prog.maxVoucherAnte = prog.maxBossAnte = 1;
  Instance inst(seed);
  SeedFacts facts;
  facts.reset(&prog, &inst);
  if (prog.gd->weighted) {
    facts.needStart(1);
  } else {
    facts.needTags();
    facts.needVouchers();
    facts.needBosses();
  }
  Start s;
  s.small = facts.small[1], s.big = facts.big[1];
  s.voucher = facts.voucher[1], s.boss = facts.boss[1];
  return s;
}

std::string FilterProgram::describe(std::shared_ptr<const GameData> data, Seed &seed,
                                    int antes, int cards, int packs) {
  FilterProgram prog;
  prog.gd = data ? data : GameData::vanilla();
  prog.resolve();
  const GameData &d = *prog.gd;
  antes = std::max(1, std::min(antes, MAX_ANTE));
  cards = std::max(0, std::min(cards, 64));
  packs = std::max(0, std::min(packs, 32));
  for (int a = 1; a <= antes; a++) {
    prog.tagAnte[a] = true;
    prog.jokerEditions[a] = true;
    std::fill(prog.jokerRarities[a].begin(), prog.jokerRarities[a].end(), 1);
    prog.bufSize[a] = 2;
    prog.bufEditions[a] = true;
    for (int i = 0; i < GameData::CARD_SETS; i++) prog.cardShop[a][i] = true;
    prog.cardPackSize[a][Condition::ARCANA] = 5;
    prog.cardPackSize[a][Condition::CELESTIAL] = 5;
    prog.cardPackSize[a][Condition::SPECTRAL] = 4;
    std::fill(prog.bufRarities[a].begin(), prog.bufRarities[a].end(), 1);
  }
  prog.maxTagAnte = prog.maxVoucherAnte = prog.maxBossAnte = antes;
  Instance inst(seed);
  SeedFacts facts;
  facts.reset(&prog, &inst);
  if (!d.weighted) {
    facts.needTags();
    facts.needVouchers();
    facts.needBosses();
  }
  auto key = [&](int id) {
    return id == NONE ? std::string("-") : GameData::encodeWord(d.keyOf(id));
  };
  std::string out;
  for (int a = 1; a <= antes; a++) {
    if (d.weighted) facts.needStart(a);
    out += "ante=" + std::to_string(a) + " small=" + key(facts.small[a]) +
           " big=" + key(facts.big[a]) + " voucher=" + key(facts.voucher[a]) +
           " boss=" + key(facts.boss[a]) + " shop=";
    const std::vector<SeedFacts::Card> &shop = facts.needShop(a, cards);
    for (int i = 0; i < cards; i++)
      out += (i ? "," : "") + key(shop[i].joker) +
             (shop[i].joker != NONE && shop[i].edition != NONE
                  ? "/" + key(shop[i].edition) : "");
    out += " packs=";
    const std::vector<int> &list = facts.needPacks(a, packs);
    for (int i = 0; i < packs; i++) out += (i ? "," : "") + key(list[i]);
    out += " legend=";
    for (int k = 1; k <= 2; k++) {
      int edition = facts.soulEdition(a, k);
      out += (k > 1 ? "," : "") + key(d.weighted ? facts.legendInAnte(a, k) : facts.legend(k)) +
             (edition != NONE ? "/" + key(edition) : "");
    }
    static const char *const NAMES[4] = {"", " arcana=", " celestial=", " spectralpacks="};
    for (int f = Condition::ARCANA; f <= Condition::SPECTRAL; f++) {
      out += NAMES[f];
      const std::vector<std::vector<int>> &packs = facts.needCardPacks(a, f, 2);
      for (int p = 0; p < 2; p++)
        for (size_t i = 0; i < packs[p].size(); i++)
          out += (p || i ? (i ? "," : "|") : "") + key(packs[p][i]);
    }
    {
      out += " buffoon=";
      const std::vector<SeedFacts::Card> &pack = facts.needBuffoon(a, 4);
      for (int i = 0; i < 4; i++)
        out += (i ? "," : "") + key(pack[i].joker) +
               (pack[i].edition != NONE ? "/" + key(pack[i].edition) : "");
    }
    out += "\n";
  }
  return out;
}
