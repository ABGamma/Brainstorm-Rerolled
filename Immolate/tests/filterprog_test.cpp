// filterprog_test.cpp — custom filters (src/filterprog.cpp).
//
// 1. parse:    the text format: defaults, options, comments, ';', and a clear
//              error for every kind of mistake.
// 2. fast:     FilterProgram::matches() == AND of referenceCheck() over its
//              conditions, for thousands of random programs on many seeds.
//              Conditions are built to be TRUE for a base seed (from the full
//              generators), so both outcomes are exercised.
// 3. search:   through the C API: the prefiltered multi-threaded search finds
//              the same first seed as a plain single-threaded scan, and
//              bs_check_seed agrees with the reference.

#include "brainstorm_api.h"
#include "filterprog.hpp"
#include "filters.hpp"
#include "gamedata.hpp"
#include "functions.hpp"
#include "prefilter.hpp"
#include "scan.hpp"

#include <algorithm>
#include <cstdio>
#include <random>
#include <fstream>
#include <sstream>
#include <string>

static int failures = 0;

static void report(bool ok, const std::string &what, long long checked = 1,
                   long long wrong = 0) {
  std::printf("%s  %-52s %8lld checked, %lld wrong\n", ok ? "PASS" : "FAIL",
              what.c_str(), checked, wrong);
  failures += !ok;
}

// ---------------------------------------------------------------------------
// 1. Parsing

static void testParse() {
  std::string err;
  auto p = FilterProgram::parse(
      "# a comment\n"
      "tag tag_charm\n"
      "  joker j_blueprint ante=2 within=4 edition=e_negative ; voucher v_hone ante=3\n"
      "pack p_arcana_mega within=3\n"
      "soul packs=2 size=3 legendary=j_perkeo ante=1\n"
      "tag tag_double blind=big ante=4\n"
      "assume showman\n",
      err);
  bool ok = p && p->conditions().size() == 6;
  if (ok) {
    const auto &c = p->conditions();
    ok = c[0].kind == Condition::TAG && c[0].item == Item::Charm_Tag &&
         c[0].ante == 1 && c[0].blind == 0 &&
         c[1].kind == Condition::JOKER && c[1].item == Item::Blueprint &&
         c[1].ante == 2 && c[1].within == 4 && c[1].edition == Item::Negative &&
         !c[1].anyEdition && p->data().keyOf(c[1].editionId) == "e_negative" &&
         p->data().keyOf(c[1].id) == "j_blueprint" && p->assumesShowman() &&
         c[2].kind == Condition::VOUCHER && c[2].item == Item::Hone && c[2].ante == 3 &&
         c[3].kind == Condition::PACK && c[3].item == Item::Mega_Arcana_Pack &&
         c[3].within == 3 && c[3].ante == 1 &&
         c[4].kind == Condition::SOUL && c[4].packs == 2 && c[4].size == 3 &&
         c[4].item == Item::Perkeo &&
         c[5].blind == 2 && c[5].ante == 4 && c[5].item == Item::Double_Tag;
  }
  report(ok, "parse: fields, defaults, comments, ';'");

  p = FilterProgram::parse("joker j_ticket ante=2\nassume enhanced", err);
  report(p && p->assumesEnhanced() && p->conditions().size() == 1,
         "parse: 'assume enhanced' (any line) allows gated jokers");

  p = FilterProgram::parse("", err);
  report(p && p->conditions().empty(), "parse: empty text = no conditions");

  p = FilterProgram::parse("joker j_burglar ante=6-8 within=100\nvoucher v_retcon ante=1-8", err);
  report(p && p->conditions().size() == 2 && p->conditions()[0].ante == 6 &&
             p->conditions()[0].anteTo == 8 && p->conditions()[1].ante == 1 &&
             p->conditions()[1].anteTo == 8,
         "parse: ante ranges");

  p = FilterProgram::parse("tag tag_negative ante=1-2\nvoucher v_observatory ante=2\n"
                           "pack p_arcana_mega within=2\npack p_buffoon_normal within=1\n"
                           "soul legendary=j_perkeo\nsoul ante=2 legendary=j_triboulet",
                           err);
  report(p && p->conditions().size() == 6, "parse: the possible versions are accepted");

  // Every mistake gives an error naming the line.
  const char *bad[][2] = {
      {"tag tag_charmm", "line 1: unknown tag 'tag_charmm'"},
      {"\njoker j_perkeo", "line 2: 'j_perkeo' is legendary"},
      {"joker j_blueprint ante=9", "line 1: bad value for ante: '9'"},
      {"joker j_blueprint ante=0", "line 1: bad value for ante: '0'"},
      {"joker j_blueprint within=x", "line 1: bad value for within"},
      {"joker j_blueprint edition=e_shiny", "line 1: bad value for edition"},
      {"joker j_blueprint blind=small", "line 1: 'blind' is not an option of 'joker'"},
      {"tag tag_charm blind=huge", "line 1: bad value for blind"},
      {"voucher tag_charm", "line 1: unknown voucher 'tag_charm'"},
      {"pack p_arcana_mega_1", "line 1: unknown pack 'p_arcana_mega_1'"},
      {"soul legendary=j_blueprint", "line 1: bad value for legendary"},
      {"soul size=4", "line 1: bad value for size: '4' (Arcana packs have 3 or 5 cards)"},
      {"soul from=spectral size=5", "line 1: bad value for size: '5' (Spectral packs have 2 or 4 cards)"},
      {"soul from=celestial", "line 1: bad value for from"},
      {"soul size=5; soul from=spectral", "soul conditions in the same ante must use the same kind of pack (Arcana or Spectral)"},
      {"soul size=5; soul size=3", "soul conditions in the same ante must use the same pack size"},
      {"shop j_blueprint", "line 1: unknown condition 'shop'"},
      {"joker", "line 1: 'joker' needs an item key"},
      {"joker j_blueprint 4", "line 1: expected name=value, got '4'"},
      {"joker j_steel_joker", "line 1: 'j_steel_joker' only appears with an enhanced card"},
      {"joker j_cavendish; assume enhanced", "line 1: 'j_cavendish' can't appear in the shops of a fresh run"},
      // Conditions that can never hold are errors, not endless searches.
      {"tag tag_negative ante=1", "line 1: 'tag_negative' can't be a skip tag on ante 1 (it is available from ante 2 on)"},
      {"voucher v_observatory", "line 1: 'v_observatory' can't be in the shop on ante 1 (it needs v_telescope bought first, so ante 2 at the earliest)"},
      {"pack p_arcana_mega within=1", "line 1: 'p_arcana_mega' can't be among the first 1 packs of ante 1 (the first pack of ante 1 is always p_buffoon_normal)"},
      {"soul packs=2 legendary=j_perkeo", "line 1: 'j_perkeo' can't come from 2 Souls in one run without Showman"},
      {"soul legendary=j_perkeo\nsoul ante=2 legendary=j_perkeo", "line 2: 'j_perkeo' can't come from The Soul twice in one run without Showman"},
      {"soul legendary=j_perkeo\nsoul legendary=j_perkeo", "line 2: 'j_perkeo' can't come from The Soul twice in one run without Showman"},
      {"assume everything", "line 1: expected 'assume enhanced'"},
      {"joker j_joker packs=2", "line 1: 'packs' is for jokers in Buffoon packs (add from=buffoon)"},
      {"joker j_joker from=buffoon within=3", "line 1: 'within' is for shop jokers; for Buffoon packs use packs= and size="},
      {"card c_nope", "line 1: unknown card 'c_nope'"},
      {"card c_temperance from=celestial", "line 1: 'c_temperance' can't come from Celestial packs"},
      {"card c_soul from=shop", "line 1: 'c_soul' can't come from the shop"},
      {"card c_black_hole from=arcana", "line 1: 'c_black_hole' can't come from Arcana packs"},
      {"card c_planet_x from=shop", "line 1: 'c_planet_x' can't appear in a fresh run"},
      {"card c_familiar from=shop", "line 1: 'c_familiar' can't be in the shop of a fresh run (Spectral cards are only sold with the Ghost Deck)"},
      {"card c_fool from=shop packs=2", "line 1: 'packs' is for cards in packs; in the shop use within="},
      {"card c_fool within=3", "line 1: 'within' is for cards in the shop; in packs use packs= and size="},
      {"card c_fool size=4", "line 1: bad value for size: '4' (Arcana packs have 3 or 5 cards)"},
      {"card c_ankh from=spectral size=5", "line 1: bad value for size: '5' (Spectral packs have 2 or 4 cards)"},
      {"card c_fool size=3\ncard c_star size=5", "card conditions in the same ante must use the same pack size"},
      {"soul size=5\ncard c_fool size=3", "soul and card conditions in the same ante must use the same size of Arcana pack"},
      {"card c_fool edition=e_foil", "line 1: 'edition' is not an option of 'card'"},
      {"joker j_joker from=buffoon size=3", "line 1: bad value for size: '3' (Buffoon packs have 2 or 4 cards)"},
      {"joker j_joker from=buffoon\njoker j_egg from=buffoon size=4", "Buffoon pack jokers in the same ante must use the same pack size"},
      {"tag tag_charm ante=5-3", "line 1: bad value for ante: '5-3'"},
      {"tag tag_charm ante=1-9", "line 1: bad value for ante: '1-9'"},
      {"soul ante=1-2 size=5; soul ante=2 size=3", "soul conditions in the same ante must use the same pack size"},
      {"boss bl_nope", "line 1: unknown boss 'bl_nope'"},
      {"boss bl_final_heart ante=3", "line 1: 'bl_final_heart' can't be the boss of ante 3 (only of ante 8)"},
      {"boss bl_ox ante=2-5", "line 1: 'bl_ox' can't be the boss of antes 2-5 (only of ante 6, 7)"},
      {"boss bl_club ante=8", "line 1: 'bl_club' can't be the boss of ante 8"},
      {"boss bl_club within=3", "line 1: 'within' is not an option of 'boss'"},
  };
  int wrong = 0;
  for (auto &b : bad) {
    p = FilterProgram::parse(b[0], err);
    if (p || err.rfind(b[1], 0) != 0) {
      if (wrong++ < 5)
        std::printf("    '%s' -> %s\n", b[0], p ? "accepted" : err.c_str());
    }
  }
  report(wrong == 0, "parse: errors name the line and the problem",
         sizeof bad / sizeof bad[0], wrong);
}

// ---------------------------------------------------------------------------
// 2. Fast check == reference

// Items written as game keys ("j_blueprint") when building condition text.
std::ostream &operator<<(std::ostream &o, Item item) { return o << gameKeyOf(item); }

// A condition (as text) that is true for `seed`, built with the full
// generators. Random choices come from `rng`.
static const Item GATED[] = {Item::Steel_Joker, Item::Stone_Joker, Item::Lucky_Cat,
                             Item::Golden_Ticket, Item::Glass_Joker};

static std::string trueCondition(Seed &seed, std::mt19937_64 &rng,
                                 bool assume, bool showman) {
  int kind = (int)(rng() % 6), ante = 1 + (int)(rng() % 8);
  if (rng() % 2) ante = 1 + (int)(rng() % 2); // antes 1-2 are the usual case
  std::ostringstream out;
  Instance inst(seed);
  inst.initLocks(1, false, true);
  if (assume)
    for (Item j : GATED) inst.unlock(j);
  switch (kind) {
  case 0: { // tag
    for (int a = 2; a <= ante; a++) inst.initUnlocks(a, false);
    Item small = inst.nextTag(ante), big = inst.nextTag(ante);
    int blind = (int)(rng() % 3);
    Item t = blind == 2 ? big : blind == 1 ? small : (rng() % 2 ? small : big);
    const char *blinds[] = {"any", "small", "big"};
    out << "tag " << t << " ante=" << ante << " blind=" << blinds[blind];
    break;
  }
  case 1: { // voucher
    for (int a = 1; a < ante; a++) inst.activateVoucher(inst.nextVoucher(a));
    out << "voucher " << inst.nextVoucher(ante) << " ante=" << ante;
    break;
  }
  case 2: { // pack
    if (ante >= 2) inst.cache.generatedFirstPack = true;
    int within = 1 + (int)(rng() % 6), pick = (int)(rng() % within);
    Item p = Item::RETRY;
    for (int i = 0; i <= pick; i++) p = inst.nextPack(ante);
    out << "pack " << p << " ante=" << ante << " within=" << within;
    break;
  }
  case 3: { // joker
    int within = 1 + (int)(rng() % 40);
    std::vector<ShopItem> jokers;
    for (int i = 0; i < within; i++) {
      ShopItem it = inst.nextShopItem(ante);
      if (it.type == Item::T_Joker) jokers.push_back(it);
    }
    if (jokers.empty()) { // no joker: a (probably false) random one
      out << "joker " << (assume ? GATED[rng() % 5] : Item::Blueprint)
          << " ante=" << ante << " within=" << within;
      break;
    }
    // With the assumption, prefer a gated joker when the shop has one.
    if (assume)
      for (ShopItem &j : jokers)
        for (Item g : GATED)
          if (j.item == g && rng() % 2) {
            out << "joker " << j.item << " ante=" << ante
                << " within=" << within;
            return out.str();
          }
    ShopItem &j = jokers[rng() % jokers.size()];
    out << "joker " << j.item << " ante=" << ante << " within=" << within;
    if (rng() % 2) out << " edition=" << j.jokerData.edition;
    break;
  }
  case 4: { // boss
    Item b = Item::RETRY;
    for (int a = 1; a <= ante; a++) b = inst.nextBoss(a);
    // Sometimes another boss that is possible on that ante (usually false).
    if (rng() % 3 == 0) {
      Item other = BOSSES[rng() % BOSSES.size()];
      if (bossPossible(other, ante)) b = other;
    }
    out << "boss " << b << " ante=" << ante;
    break;
  }
  default: { // soul (rarely true; also exercises the "false" side)
    int size = rng() % 2 ? 5 : 3;
    // Without Showman one legendary can't come from two Souls (an error).
    bool legendary = rng() % 3 == 0;
    int packs = legendary && !showman ? 1 : 1 + (int)(rng() % 2);
    out << "soul ante=" << ante << " packs=" << packs << " size=" << size;
    if (legendary) out << " legendary=" << LEGENDARY_JOKERS[rng() % 5];
    break;
  }
  }
  return out.str();
}



static void testFast(int programs, int seedsPerProgram) {
  std::string roundTripError;
  auto roundTrip = GameData::parse(GameData::vanilla()->toText(), roundTripError);
  if (!roundTrip) {
    std::printf("    %s\n", roundTripError.c_str());
    report(false, "game data: built-in lists survive text and back");
    return;
  }
  report(roundTrip->toText() == GameData::vanilla()->toText(),
         "game data: built-in lists survive text and back");
  std::mt19937_64 rng(2026);
  long long checked = 0, wrong = 0, passes = 0, conditions = 0, gated = 0;
  for (int k = 0; k < programs; k++) {
    long long base = 1 + (long long)(rng() % 2318107019760ull);
    Seed baseSeed(base);
    // 1-6 conditions, mostly true for the base seed; several often share a
    // kind and ante (the case where the fast check shares work).
    int n = 1 + (int)(rng() % 6);
    bool assume = rng() % 3 == 0, showman = rng() % 4 == 0;
    std::string text = assume ? "assume enhanced\n" : "";
    if (showman) text += "assume showman\n";
    for (int i = 0; i < n; i++) {
      std::string line = trueCondition(baseSeed, rng, assume, showman);
      // Sometimes widen "ante=N" to a range containing N (still true).
      size_t at = line.find(" ante=");
      if (rng() % 3 == 0 && at != std::string::npos) {
        int a = line[at + 6] - '0';
        int lo = 1 + (int)(rng() % a), hi = a + (int)(rng() % (9 - a));
        line.replace(at + 6, 1, std::to_string(lo) + "-" + std::to_string(hi));
      }
      text += line + "\n";
    }
    std::string err;
    auto prog = FilterProgram::parse(text, err);
    if (!prog) {
      // Two soul conditions in one ante with different sizes: not allowed.
      if (err.find("same pack size") != std::string::npos) continue;
      // The same legendary asked of two antes' Souls: not allowed either.
      if (err.find("twice in one run") != std::string::npos) continue;
      std::printf("    parse failed: %s\n%s", err.c_str(), text.c_str());
      failures++;
      continue;
    }
    conditions += prog->conditions().size();
    for (const Condition &c : prog->conditions())
      for (Item g : GATED) gated += c.item == g;
    // The same filter on the built-in lists written out as text and read
    // back (what a mod sends) must behave identically.
    auto sent = FilterProgram::parse(text, err, roundTrip);
    if (!sent) {
      std::printf("    parse failed on sent game data: %s\n%s", err.c_str(), text.c_str());
      failures++;
      continue;
    }
    for (int s = 0; s < seedsPerProgram; s++) {
      Seed seed(base + s); // the base seed itself, then its neighbours
      bool expect = true;
      for (const Condition &c : prog->conditions())
        expect = expect && prog->referenceCheck(c, seed);
      Seed again(base + s);
      bool got = prog->matches(again);
      Seed third(base + s);
      if (sent->matches(third) != got) got = !expect; // counts as wrong
      checked++;
      passes += expect;
      if (got != expect && wrong++ < 5)
        std::printf("    %s: fast %d, reference %d for\n%s",
                    seed.tostring().c_str(), got, expect, text.c_str());
    }
  }
  report(wrong == 0,
         "fast == reference (" + std::to_string(conditions) +
             " conditions, " + std::to_string(gated) + " on gated jokers, " +
             std::to_string(passes) + " pass)",
         checked, wrong);
}

// Soul conditions on several antes are one run: legendaries come in order.
// Rare events, so whole ranges of seeds are compared with the reference.
static void testSoulOrder(long long seeds) {
  const char *specs[] = {
      "soul ante=1\nsoul ante=2 legendary=j_perkeo",
      "soul ante=1 legendary=j_perkeo\nsoul ante=3 legendary=j_triboulet",
      "assume showman\nsoul packs=2 legendary=j_perkeo",
      "soul ante=2\nsoul ante=2 legendary=j_yorick",
      // The Soul joker's edition: one roll per Soul on its ante's own
      // stream ("edisou" + ante), counted from the ante's first Soul.
      "soul ante=1 edition=e_negative",
      "soul ante=1 edition=e_base",
      "soul ante=1 legendary=j_perkeo edition=e_foil",
      "soul ante=1\nsoul ante=2 edition=e_base",
      "soul ante=1 edition=e_base\nsoul ante=2 edition=e_holo",
      "assume showman\nsoul packs=2 edition=e_base",
      "soul ante=1-3 edition=e_polychrome",
      // Spectral packs: The Soul and the Black Hole roll on one stream.
      "soul ante=1 from=spectral",
      "soul ante=2 from=spectral size=2 packs=2",
      "assume showman\nsoul from=spectral packs=2",
      "soul ante=1\nsoul ante=2 from=spectral size=2 legendary=j_perkeo",
      "soul ante=1 from=spectral edition=e_base\nsoul ante=2 edition=e_base",
      "soul ante=1-3 from=spectral packs=2",
      // Soul conditions on one ante take its Souls one after another.
      "soul ante=1\nsoul ante=1 edition=e_base",
      "soul ante=2 edition=e_base\nsoul ante=1\nsoul ante=2",
      "soul ante=1 from=spectral size=2\nsoul ante=1 from=spectral size=2 edition=e_base",
      "assume showman\nsoul ante=1-3\nsoul ante=1-3 edition=e_base",
      "soul ante=1 legendary=j_perkeo\nsoul ante=1",
  };
  for (const char *spec : specs) {
    std::string err;
    auto prog = FilterProgram::parse(spec, err);
    if (!prog) {
      report(false, std::string("soul order parses: ") + err);
      continue;
    }
    long long wrong = 0, passes = 0;
    Seed s(700000000LL);
    Instance inst(s);
    for (long long i = 0; i < seeds; i++, inst.next()) {
      bool got = prog->matches(inst);
      bool expect = true;
      for (const Condition &c : prog->conditions()) {
        Seed one(700000000LL + i);
        expect = expect && prog->referenceCheck(c, one);
        if (!expect) break;
      }
      passes += expect;
      wrong += got != expect;
    }
    std::string name = spec;
    std::replace(name.begin(), name.end(), '\n', ';');
    report(wrong == 0 && passes > 0,
           "souls in order (" + std::to_string(passes) + " pass): " + name.substr(0, 28),
           seeds, wrong);
  }
}

// Shop jokers with "assume showman": a joker may come again in the same
// shop. Fast == reference with and without it, and Showman must change the
// answer for some seeds (otherwise the setting would do nothing for shops).
static void testShopShowman(long long seeds) {
  const char *conds[] = {
      "joker j_joker ante=1 within=12",
      "joker j_gros_michel ante=2 within=20",
      "joker j_greedy_joker ante=1-2 within=8",
  };
  for (const char *cond : conds) {
    std::string err;
    auto plain = FilterProgram::parse(cond, err);
    auto show = FilterProgram::parse(std::string("assume showman\n") + cond, err);
    if (!plain || !show) {
      report(false, std::string("shop showman parses: ") + err);
      continue;
    }
    long long wrong = 0, differ = 0, passes = 0;
    for (long long i = 0; i < seeds; i++) {
      bool results[2];
      for (int k = 0; k < 2; k++) {
        const FilterProgram &prog = k ? *show : *plain;
        Seed a(900000000LL + i), b(900000000LL + i);
        bool got = prog.matches(a);
        bool expect = prog.referenceCheck(prog.conditions()[0], b);
        wrong += got != expect;
        results[k] = expect;
      }
      passes += results[1];
      differ += results[0] != results[1];
    }
    report(wrong == 0 && differ > 0,
           std::string("shop showman (") + std::to_string(differ) +
               " seeds differ): " + cond,
           seeds * 2, wrong);
  }
}

// Jokers in Buffoon packs: fast == reference, with and without Showman
// (which lets a joker come twice in one pack).
static void testBuffoon(long long seeds) {
  const char *specs[] = {
      "joker j_joker from=buffoon",
      "joker j_joker from=buffoon packs=3",
      "joker j_greedy_joker ante=2 from=buffoon size=4",
      "joker j_blueprint ante=1-3 from=buffoon packs=2 size=4",
      "joker j_joker from=buffoon edition=e_foil packs=2",
      "joker j_joker from=buffoon edition=e_base size=4",
      "joker j_joker from=buffoon\njoker j_greedy_joker from=buffoon packs=2",
      "assume showman\njoker j_joker from=buffoon size=4",
      "assume enhanced\njoker j_stone from=buffoon size=4 packs=2",
      "joker j_joker from=buffoon\njoker j_joker ante=1 within=4",
  };
  for (const char *spec : specs) {
    std::string err;
    auto prog = FilterProgram::parse(spec, err);
    if (!prog) {
      report(false, std::string("buffoon parses: ") + err);
      continue;
    }
    long long wrong = 0, passes = 0;
    Seed s(800000000LL);
    Instance inst(s);
    for (long long i = 0; i < seeds; i++, inst.next()) {
      bool got = prog->matches(inst);
      bool expect = true;
      for (const Condition &c : prog->conditions()) {
        Seed one(800000000LL + i);
        expect = expect && prog->referenceCheck(c, one);
        if (!expect) break;
      }
      passes += expect;
      wrong += got != expect;
    }
    std::string name = spec;
    std::replace(name.begin(), name.end(), '\n', ';');
    report(wrong == 0 && passes > 0,
           "buffoon (" + std::to_string(passes) + " pass): " + name.substr(0, 34),
           seeds, wrong);
  }
}

// Tarot, Planet and Spectral cards in packs and in the shop: fast ==
// reference (the reference opens the packs with Instance::nextTarot, ...).
static void testCards(long long seeds) {
  const char *specs[] = {
      "card c_temperance",
      "card c_temperance packs=3 size=3",
      "card c_fool ante=2 packs=2",
      "card c_pluto",
      "card c_jupiter ante=3 from=celestial size=3 packs=2",
      "card c_black_hole",
      "card c_black_hole from=spectral packs=2",
      "card c_soul",
      "card c_soul from=spectral size=2 packs=3",
      "card c_familiar from=spectral",
      "card c_cryptid ante=1-3 from=spectral size=2",
      "card c_hermit from=shop within=6",
      "card c_jupiter ante=2 from=shop within=10",
      "card c_temperance ante=1-3 from=shop within=4",
      "assume showman\ncard c_fool packs=2",
      "assume showman\ncard c_hermit from=shop within=8",
      "soul ante=1\ncard c_temperance ante=1",
      "soul ante=1 from=spectral\ncard c_ankh ante=1 from=spectral",
      "joker j_joker within=6\ncard c_star from=shop within=6",
      "card c_star from=shop within=10\ncard c_moon from=shop within=10\ncard c_mars from=shop within=10",
      "card c_lovers\ncard c_tower\ncard c_neptune\ncard c_aura",
  };
  for (const char *spec : specs) {
    std::string err;
    auto prog = FilterProgram::parse(spec, err);
    if (!prog) {
      report(false, std::string("cards parse: ") + err);
      continue;
    }
    long long wrong = 0, passes = 0;
    Seed s(600000000LL);
    Instance inst(s);
    for (long long i = 0; i < seeds; i++, inst.next()) {
      bool got = prog->matches(inst);
      bool expect = true;
      for (const Condition &c : prog->conditions()) {
        Seed one(600000000LL + i);
        expect = expect && prog->referenceCheck(c, one);
        if (!expect) break;
      }
      passes += expect;
      wrong += got != expect;
    }
    std::string name = spec;
    std::replace(name.begin(), name.end(), '\n', ';');
    report(wrong == 0 && passes > 0,
           "cards (" + std::to_string(passes) + " pass): " + name.substr(0, 36), seeds, wrong);
  }
}

// Game data sent as text: mistakes, a renamed (i.e. "modded") joker, lists
// without placeholder slots, and the start-of-run prediction.
static void testGameData(int seeds) {
  std::string err;
  const char *bad[][2] = {
      {"", "game data, line 1: expected 'gamedata 1'"},
      {"gamedata 2", "game data, line 1: expected 'gamedata 1' (unsupported game data version?)"},
      {"gamedata 1\nwhat 3", "game data, line 2: unknown or malformed record 'what'"},
      {"gamedata 1\npack 0.5 p_a", "game data, line 2: pack steps must start at 0 and rise"},
      {"gamedata 1\nshop 10 20 2", "game data, line 2: bad shop rates"},
      {"gamedata 1\nboss bl_hook 129", "game data, line 2: expected: boss <key> <antes as digits, or ->"},
      {"gamedata 1\nmodsoul c_x 0.003 0 Joker", "game data, line 2: expected: modsoul <key> <rate> <0|1> <Tarot|Planet|Spectral>..."},
      {"gamedata 1\ncards Code c_x", "game data, line 2: expected: cards <Tarot|Planet|Spectral> <list>"},
  };
  int wrong = 0;
  for (auto &b : bad) {
    auto d = GameData::parse(b[0], err);
    if (d || err != b[1]) {
      wrong++;
      std::printf("    '%s' -> %s\n", b[0], d ? "accepted" : err.c_str());
    }
  }
  report(wrong == 0, "game data: mistakes name the line", sizeof bad / sizeof bad[0], wrong);

  // A joker the library has never heard of, in the place of j_joker.
  std::string text = GameData::vanilla()->toText();
  size_t at = text.find(" j_joker ");
  text.replace(at, 9, " j_mod_new ");
  auto modded = GameData::parse(text, err);
  auto plain = FilterProgram::parse("joker j_joker within=6\ntag tag_charm blind=small", err);
  auto renamed = modded ? FilterProgram::parse("joker j_mod_new within=6\ntag tag_charm blind=small", err, modded) : nullptr;
  auto unknown = modded ? FilterProgram::parse("joker j_joker", err, modded) : nullptr;
  long long differ = 0, passes = 0;
  if (plain && renamed)
    for (int i = 0; i < seeds * 20; i++) {
      Seed a(900000000LL + i), b(900000000LL + i);
      bool x = plain->matches(a);
      differ += x != renamed->matches(b);
      passes += x;
    }
  report(plain && renamed && !unknown && differ == 0 && passes > 0,
         "game data: a modded joker is found like any other", seeds * 20, differ);

  // A modded key with a space in it (Cryptid has "j_cry_Double Scale"):
  // percent-encoded in the text, so it stays one entry and the lists keep
  // their length. Same seeds as above: must match exactly like j_joker.
  bool words = GameData::decodeWord(GameData::encodeWord("a b%c\t")) == "a b%c\t" &&
               GameData::encodeWord("j_cry_Double Scale") == "j_cry_Double%20Scale" &&
               GameData::decodeWord("100%") == "100%" && GameData::decodeWord("%4") == "%4";
  std::string spaced = GameData::vanilla()->toText();
  size_t at2 = spaced.find(" j_joker ");
  spaced.replace(at2, 9, " j_mod%20two ");
  auto withSpace = GameData::parse(spaced, err);
  bool roundTrip = withSpace && withSpace->toText().find(" j_mod%20two ") != std::string::npos;
  auto spacedProg = withSpace
      ? FilterProgram::parse("joker j_mod%20two within=6\ntag tag_charm blind=small", err, withSpace)
      : nullptr;
  long long spacedDiffer = 0;
  if (plain && spacedProg)
    for (int i = 0; i < seeds * 20; i++) {
      Seed a(900000000LL + i), b(900000000LL + i);
      spacedDiffer += plain->matches(a) != spacedProg->matches(b);
    }
  report(words && roundTrip && spacedProg && spacedDiffer == 0,
         "game data: a modded key with a space in it", seeds * 20, spacedDiffer);

  // Without placeholder slots (pad 0) the lists are shorter; nothing to
  // compare with, but it must run and find things.
  size_t pad = text.find("pad 1");
  text.replace(pad, 5, "pad 0");
  auto dropped = GameData::parse(text, err);
  auto prog = dropped ? FilterProgram::parse("joker j_mod_new within=6\nvoucher v_hone ante=1-2\ntag tag_charm", err, dropped) : nullptr;
  passes = 0;
  if (prog)
    for (int i = 0; i < seeds * 200; i++) {
      Seed a(900000000LL + i);
      passes += prog->matches(a);
    }
  report(prog && passes > 0, "game data: lists without placeholder slots", seeds * 200, 0);

  // The start of a run, against the full generators.
  wrong = 0;
  const GameData &d = *GameData::vanilla();
  for (int i = 0; i < seeds; i++) {
    Seed a(123456789LL + i), b(123456789LL + i);
    FilterProgram::Start st = FilterProgram::predictStart(GameData::vanilla(), a);
    Instance inst(b);
    inst.initLocks(1, false, true);
    Item small = inst.nextTag(1), big = inst.nextTag(1);
    bool ok = d.keyOf(st.small) == gameKeyOf(small) && d.keyOf(st.big) == gameKeyOf(big) &&
              d.keyOf(st.voucher) == gameKeyOf(inst.nextVoucher(1)) &&
              d.keyOf(st.boss) == gameKeyOf(inst.nextBoss(1));
    wrong += !ok;
  }
  char out[128];
  bs_params p{};
  p.api_version = BS_API_VERSION;
  bool api = bs_predict_start(&p, "ABCD1234", out, sizeof out) == 1 &&
             std::string(out).rfind("small=tag_", 0) == 0;
  report(wrong == 0 && api, "start of run: tags, voucher, boss", seeds, wrong);
}

// A weighted game (Steamodded "object weights", gamedata.hpp): the lists a
// real modded game exported (Pokermon 3.9.1 on Steamodded 26.1002.0, saved
// in tests/data/weighted_gamedata.txt) and what 14 real runs of that game
// started with (weighted_starts.txt: seed, both skip tags, voucher, boss,
// read from the running game).
static std::string readFile(const std::string &name) {
  std::ifstream in(std::string(TEST_DATA_DIR) + "/" + name);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

static void testWeighted(int seeds) {
  const int NONE = GameData::NONE;
  std::string err, text = readFile("weighted_gamedata.txt");
  auto data = GameData::parse(text, err);
  if (!data || !data->weighted) {
    report(false, "weighted game: the saved lists are read (" + err + ")");
    return;
  }
  // Text and back gives the same data.
  std::string once = data->toText();
  auto again = GameData::parse(once, err);
  report(again && again->toText() == once, "weighted game: lists survive text and back");

  // The start of 14 real runs.
  std::istringstream truth(readFile("weighted_starts.txt"));
  int wrong = 0, runs = 0;
  std::string seedText, small, big, voucher, boss;
  while (truth >> seedText >> small >> big >> voucher >> boss) {
    Seed seed(seedText);
    FilterProgram::Start st = FilterProgram::predictStart(data, seed);
    bool ok = data->keyOf(st.small) == small && data->keyOf(st.big) == big &&
              data->keyOf(st.voucher) == voucher && data->keyOf(st.boss) == boss;
    if (!ok)
      std::printf("    %s: predicted %s %s %s %s\n", seedText.c_str(),
                  data->keyOf(st.small).c_str(), data->keyOf(st.big).c_str(),
                  data->keyOf(st.voucher).c_str(), data->keyOf(st.boss).c_str());
    wrong += !ok;
    runs++;
    // The same through a filter: it must accept this seed.
    auto prog = FilterProgram::parse("tag " + small + " blind=small\ntag " + big +
                                         " blind=big\nvoucher " + voucher + "\nboss " + boss,
                                     err, data);
    Seed same(seedText);
    if (!prog || !prog->matches(same)) {
      std::printf("    %s: the filter for its own start says no (%s)\n", seedText.c_str(), err.c_str());
      wrong++;
    }
  }
  report(runs == 14 && wrong == 0, "weighted game: the start of 14 real runs", runs, wrong);

  // Seven whole runs played in the real game: per
  // run, ante 1 with six shop cards (two shops and a reroll) and four packs,
  // then ante 2 with two cards and two packs. "seed|ante 1|ante 2", each
  // ante in describe()'s form. The game's line may list fewer cards.
  auto fieldOf = [](const std::string &line, const std::string &name) {
    size_t at = line.find(" " + name + "=");
    if (at == std::string::npos) return std::string();
    size_t from = at + name.size() + 2, to = line.find_first_of(" \n", from);
    return line.substr(from, to == std::string::npos ? to : to - from);
  };
  std::istringstream played(readFile("weighted_runs.txt"));
  wrong = runs = 0;
  for (std::string line; std::getline(played, line);) {
    size_t bar1 = line.find('|'), bar2 = line.find('|', bar1 + 1);
    if (bar1 == std::string::npos || bar2 == std::string::npos) continue;
    Seed seed(line.substr(0, bar1));
    std::string game[2] = {" " + line.substr(bar1 + 1, bar2 - bar1 - 1), " " + line.substr(bar2 + 1)};
    std::string ours = FilterProgram::describe(data, seed, 2, 6, 4);
    std::string predicted[2] = {" " + ours.substr(0, ours.find('\n')), " " + ours.substr(ours.find('\n') + 1)};
    runs++;
    for (int a = 0; a < 2; a++)
      for (const char *name : {"small", "big", "voucher", "boss", "shop", "packs"}) {
        std::string want = fieldOf(game[a], name), got = fieldOf(predicted[a], name);
        if (want.empty() || got.compare(0, want.size(), want) != 0) {
          std::printf("    %s ante %d %s: predicted %s, the game had %s\n",
                      line.substr(0, bar1).c_str(), a + 1, name, got.c_str(), want.c_str());
          wrong++;
        }
      }
  }
  report(runs == 7 && wrong == 0, "weighted game: 7 runs the game played into ante 2", runs, wrong);

  // What the first two Souls of ante 1 give, from the game's own pick
  // function in 8 fresh runs.
  std::istringstream souls(readFile("weighted_souls.txt"));
  wrong = runs = 0;
  std::string first, second;
  while (souls >> seedText >> first >> second) {
    Seed seed(seedText);
    std::string ours = FilterProgram::describe(data, seed, 1, 0, 0);
    // (describe() adds "/edition" to a Soul joker that has one.)
    std::string legend = fieldOf(" " + ours, "legend"), plain;
    for (size_t i = 0; i < legend.size(); i++) {
      if (legend[i] == '/')
        while (i < legend.size() && legend[i] != ',') i++;
      if (i < legend.size()) plain += legend[i];
    }
    wrong += plain != first + "," + second;
    runs++;
    // The same through conditions (one run: ante 1's Soul, then ante 2's
    // would be another stream, so only the first is asked here).
    auto prog = FilterProgram::parse("soul legendary=" + first, err, data);
    wrong += !prog; // (matching also needs a Soul in the pack: not asked)
  }
  report(runs == 8 && wrong == 0, "weighted game: what The Soul gives in 8 real runs", runs, wrong);

  // Conditions and the description read the same prediction: a seed a shop
  // or pack condition accepts must show that item in describe(), and the
  // other way round.
  long long differ = 0, passes = 0;
  int joker = NONE, pack = NONE;
  for (const GameData::Entry &e : data->rarityPools[0].jokers)
    if (e.id != NONE && e.weight > 0 && joker == NONE) joker = e.id;
  for (const GameData::Entry &e : data->packList)
    if (e.id != NONE && e.id != data->firstPack && pack == NONE) pack = e.id;
  std::string jokerKey = data->keyOf(joker), packKey = data->keyOf(pack);
  auto shopProg = FilterProgram::parse("joker " + GameData::encodeWord(jokerKey) + " ante=2 within=6", err, data);
  auto packProg = FilterProgram::parse("pack " + GameData::encodeWord(packKey) + " ante=1 within=4", err, data);
  if (!shopProg || !packProg) {
    report(false, "weighted game: filters parse (" + err + ")");
    return;
  }
  for (int i = 0; i < seeds; i++) {
    Seed a(555000000LL + i), b(555000000LL + i), c(555000000LL + i);
    std::string lines = FilterProgram::describe(data, a, 2, 6, 4);
    size_t second = lines.find('\n') + 1;
    std::string ante1 = lines.substr(0, second), ante2 = lines.substr(second);
    auto listed = [](const std::string &line, const std::string &field, const std::string &key) {
      size_t at = line.find(" " + field + "=");
      std::string list = "," + line.substr(at + field.size() + 2, line.find(' ', at + 1) - at - field.size() - 2) + ",";
      return list.find("," + key + ",") != std::string::npos ||
             list.find("," + key + "/") != std::string::npos;
    };
    bool inShop = listed(ante2, "shop", GameData::encodeWord(jokerKey));
    bool inPacks = listed(ante1, "packs", GameData::encodeWord(packKey));
    differ += inShop != shopProg->matches(b);
    differ += inPacks != packProg->matches(c);
    passes += inShop + inPacks;
  }
  report(differ == 0 && passes > 0,
         "weighted game: conditions agree with the description (" + std::to_string(passes) + " pass)",
         seeds * 2, differ);
}

// Consumable and Buffoon pack conditions in a weighted game: the Pokermon lists of
// tests/data/weighted_cards_gamedata.txt (exported from a run). describe()
// gave the same shop and packs as the game itself on 33 real runs (20
// random seeds and 13 with The Soul, Black Hole or Pokermon's own
// Soul-like cards in packs) and the same Buffoon packs on 60 more, so a seed
// a condition accepts must show the card there, and the other way round.
static void testWeightedCards(int seeds) {
  std::string err;
  auto data = GameData::parse(readFile("weighted_cards_gamedata.txt"), err);
  if (!data || !data->weighted || data->modSouls.empty()) {
    report(false, "weighted cards: the saved lists are read (" + err + ")");
    return;
  }
  struct Case {
    const char *spec, *field, *key; // field of describe() that shows the card
    int ante;
  };
  const Case cases[] = {
      {"card c_temperance ante=1 packs=2", "arcana", "c_temperance", 1},
      {"card c_soul ante=2 packs=2", "arcana", "c_soul", 2},
      {"card c_pluto ante=1 packs=2", "celestial", "c_pluto", 1},
      {"card c_black_hole ante=1 from=spectral packs=2", "spectralpacks", "c_black_hole", 1},
      {"card c_ankh ante=2 from=spectral packs=2", "spectralpacks", "c_ankh", 2},
      {"card c_poke_transformation ante=1 packs=2", "spectralpacks", "c_poke_transformation", 1},
      {"card c_hermit ante=1 from=shop within=6", "shop", "c_hermit", 1},
      {"card c_jupiter ante=2 from=shop within=6", "shop", "c_jupiter", 2},
      {"joker j_poke_cleffa ante=1 from=buffoon packs=2", "buffoon", "j_poke_cleffa", 1},
      {"joker j_poke_miltank ante=2 from=buffoon packs=2", "buffoon", "j_poke_miltank", 2},
  };
  for (const Case &k : cases) {
    auto prog = FilterProgram::parse(k.spec, err, data);
    if (!prog) {
      report(false, std::string("weighted cards parse: ") + err);
      continue;
    }
    long long wrong = 0, passes = 0;
    for (int i = 0; i < seeds; i++) {
      Seed a(777000000LL + i), b(777000000LL + i);
      std::string lines = FilterProgram::describe(data, a, 2, 6, 2);
      size_t second = lines.find('\n') + 1;
      std::string line = k.ante == 1 ? lines.substr(0, second) : lines.substr(second);
      // The field's list; packs are separated by '|'.
      size_t at = line.find(std::string(" ") + k.field + "=");
      size_t from = at + std::strlen(k.field) + 2;
      std::string list = "," + line.substr(from, line.find(' ', from) - from) + ",";
      std::replace(list.begin(), list.end(), '|', ',');
      list.erase(std::remove(list.begin(), list.end(), '\n'), list.end()); // the last field ends the line
      bool shown = list.find(std::string(",") + k.key + ",") != std::string::npos ||
                   list.find(std::string(",") + k.key + "/") != std::string::npos;
      passes += shown;
      wrong += shown != prog->matches(b);
    }
    report(wrong == 0 && passes > 0,
           "weighted cards (" + std::to_string(passes) + " pass): " + k.spec, seeds, wrong);
  }
}

// ---------------------------------------------------------------------------
// 3. Through the C API

static bs_params paramsFor(const char *spec, const char *start) {
  bs_params p = {};
  p.api_version = BS_API_VERSION;
  p.start_seed = start;
  p.custom_filter = "No Filter";
  p.filter_spec = spec;
  return p;
}

static void testSearch() {
  struct Case { const char *label, *spec; };
  Case cases[] = {
      {"charm (small) + mega arcana", "tag tag_charm blind=small\npack p_arcana_mega"},
      {"double tag + hone ante 1", "tag tag_double\nvoucher v_hone"},
      {"blueprint in 6 + rare tag", "joker j_blueprint within=6\ntag tag_rare"},
      {"two jokers ante 2", "joker j_burglar ante=2 within=30; joker j_dna ante=2 within=30"},
      {"telescope then observatory", "voucher v_telescope; voucher v_observatory ante=2"},
      {"soul + 2 packs ante 1", "soul\npack p_buffoon_normal within=4"},
      {"burglar ante 6-8 (old filter)", "joker j_burglar ante=6-8 within=100"},
      {"retcon by ante 8 (old filter)", "voucher v_retcon ante=1-8"},
      {"steel joker (assume enhanced)", "assume enhanced\njoker j_steel_joker within=6\njoker j_ticket within=10"},
      {"crimson heart ante 8 + the club ante 1", "boss bl_final_heart ante=8\nboss bl_club"},
  };
  for (Case &c : cases) {
    const char *start = "FILTRA11";
    // Single-threaded scans: the prefilter (which uses the filter's ante 1
    // first rolls) must not change the first match.
    std::string err;
    FilterConfig cfg;
    cfg.program = FilterProgram::parse(c.spec, err);
    Prefilter pf(cfg);
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> checked{0};
    long long from = Seed(std::string(start)).getID();
    long long plain = scanRange(cfg, nullptr, from, 2000000, stop, checked);
    long long fast = scanRange(cfg, &pf, from, 2000000, stop, checked);

    // The real search (all threads; whichever thread finds a seed first
    // wins, so it need not be the lowest one) must find a seed that really
    // passes: by the reference meaning, and by bs_check_seed.
    bs_params p = paramsFor(c.spec, start);
    bs_job *job = bs_search_start(&p);
    char out[16] = {};
    int status = BS_RUNNING;
    while (job && (status = bs_search_poll(job, out, nullptr)) == BS_RUNNING) {
    }
    if (job) bs_search_free(job);
    bool ok = plain == fast && plain >= 0 && job && status == BS_FOUND;
    if (ok) {
      Seed s{std::string(out)};
      for (const Condition &cond : cfg.program->conditions())
        ok = ok && cfg.program->referenceCheck(cond, s);
      bs_params q = paramsFor(c.spec, "");
      ok = ok && bs_check_seed(&q, out) == 1;
    }
    auto name = [](long long id) { return id < 0 ? std::string("-") : Seed(id).tostring(); };
    std::printf("%s  search %-28s plain=%-9s prefiltered=%-9s threads=%s%s\n",
                ok ? "PASS" : "FAIL", c.label, name(plain).c_str(),
                name(fast).c_str(), out, pf.active() ? "" : " (no prefilter)");
    failures += !ok;
  }

  // Errors reach bs_last_error; version 1 callers never read filter_spec.
  bs_params bad = paramsFor("joker j_nope", "");
  bool ok = bs_search_start(&bad) == nullptr &&
            std::string(bs_last_error()).find("unknown joker 'j_nope'") !=
                std::string::npos;
  report(ok, "API: a bad filter is reported via bs_last_error");
  bs_params v1 = paramsFor("garbage that would not parse", "");
  v1.api_version = 1;
  report(bs_check_seed(&v1, "AAAAAAAA") >= 0, "API: version 1 ignores filter_spec");
}

// ---------------------------------------------------------------------------
// 4. The old built-in menu filters, rebuilt as conditions (the starter
//    filters the mod offers) give the same verdict as the original code.

static void testPresets(int seeds) {
  struct Preset {
    const char *label;
    FilterConfig legacy;
    const char *spec;
  };
  std::vector<Preset> presets;
  FilterConfig c;
  c = FilterConfig(); c.tag = Item::Charm_Tag; c.souls = 1;
  presets.push_back({"charm + 1 soul", c, "tag tag_charm blind=small\nsoul"});
  c = FilterConfig(); c.tag = Item::Charm_Tag; c.souls = 1; c.perkeo = true;
  presets.push_back({"charm + soul + perkeo", c, "tag tag_charm blind=small\nsoul legendary=j_perkeo"});
  c = FilterConfig(); c.souls = 2;
  presets.push_back({"2 souls", c, "soul packs=2"});
  c = FilterConfig(); c.observatory = true;
  presets.push_back({"observatory", c, "voucher v_telescope\nvoucher v_observatory ante=2"});
  c = FilterConfig(); c.bean = true;
  presets.push_back({"ante 8 bean", c, "joker j_turtle_bean ante=8 within=150"});
  c = FilterConfig(); c.retcon = true;
  presets.push_back({"early retcon", c, "voucher v_retcon ante=1-8"});
  c = FilterConfig(); c.pack = Item::Mega_Arcana_Pack; c.voucher = Item::Hone;
  presets.push_back({"pack + voucher", c, "pack p_arcana_mega within=2\nvoucher v_hone"});
  // The old menu filters never knew that a joker can't be in one shop
  // twice (the game re-rolls the second; conditions model it). With shops
  // of one card there is no "twice", and the two must agree exactly.
  std::string err1, oneCard = GameData::vanilla()->toText();
  size_t shopAt = oneCard.find("shop 28 20 2");
  if (shopAt != std::string::npos) oneCard.replace(shopAt, 12, "shop 28 20 1");
  auto oneCardShops = GameData::parse(oneCard, err1);
  report(oneCardShops && shopAt != std::string::npos, "game data: shops of one card");
  for (Preset &p : presets) {
    std::string err;
    FilterConfig custom;
    custom.program = FilterProgram::parse(p.spec, err, oneCardShops);
    long long wrong = 0, hits = 0;
    Seed seed(std::string("PRESET11"));
    Instance inst(seed);
    for (int k = 0; k < seeds; k++) {
      bool a = brainstormFilter(inst, p.legacy) >= 1;
      bool b = brainstormFilter(inst, custom) >= 1;
      hits += a;
      if (a != b && wrong++ < 3)
        std::printf("    %s: old %d, conditions %d\n", seed.tostring().c_str(), a, b);
      inst.next();
    }
    report(wrong == 0 && custom.program,
           std::string("old filter = conditions: ") + p.label + " (" +
               std::to_string(hits) + " pass)",
           seeds, wrong);
  }
}

int main(int argc, char **argv) {
  bool quick = argc > 1 && std::string(argv[1]) == "quick";
  testParse();
  testFast(quick ? 400 : 4000, 40);
  testSoulOrder(quick ? 300000 : 1500000);
  testShopShowman(quick ? 30000 : 200000);
  testBuffoon(quick ? 30000 : 300000);
  testCards(quick ? 30000 : 300000);
  testGameData(quick ? 2000 : 20000);
  testWeighted(quick ? 3000 : 30000);
  testWeightedCards(quick ? 3000 : 30000);
  testSearch();
  testPresets(quick ? 30000 : 300000);
  std::printf("\n%s (%d failing check%s)\n", failures ? "FAILED" : "ALL PASSED",
              failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
