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
#include "functions.hpp"
#include "prefilter.hpp"
#include "scan.hpp"

#include <cstdio>
#include <random>
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
      "tag tag_double blind=big ante=4\n",
      err);
  bool ok = p && p->conditions().size() == 6;
  if (ok) {
    const auto &c = p->conditions();
    ok = c[0].kind == Condition::TAG && c[0].item == Item::Charm_Tag &&
         c[0].ante == 1 && c[0].blind == 0 &&
         c[1].kind == Condition::JOKER && c[1].item == Item::Blueprint &&
         c[1].ante == 2 && c[1].within == 4 && c[1].edition == Item::Negative &&
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
      {"soul size=4", "line 1: bad value for size"},
      {"soul size=5; soul size=3", "soul conditions in the same ante must use the same pack size"},
      {"shop j_blueprint", "line 1: unknown condition 'shop'"},
      {"joker", "line 1: 'joker' needs an item key"},
      {"joker j_blueprint 4", "line 1: expected name=value, got '4'"},
      {"joker j_steel_joker", "'j_steel_joker' only appears with an enhanced card"},
      {"joker j_cavendish; assume enhanced", "'j_cavendish' only appears after Gros Michel dies"},
      {"assume everything", "line 1: expected 'assume enhanced'"},
      {"tag tag_charm ante=5-3", "line 1: bad value for ante: '5-3'"},
      {"tag tag_charm ante=1-9", "line 1: bad value for ante: '1-9'"},
      {"soul ante=1-2 size=5; soul ante=2 size=3", "soul conditions in the same ante must use the same pack size"},
      {"boss bl_nope", "line 1: unknown boss 'bl_nope'"},
      {"boss bl_final_heart ante=3", "'bl_final_heart' can't be the boss of ante 3 (final bosses only appear on ante 8)"},
      {"boss bl_ox ante=2-5", "'bl_ox' can't be the boss of antes 2-5 (it appears from ante 6 on, but never on ante 8)"},
      {"boss bl_club ante=8", "'bl_club' can't be the boss of ante 8"},
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
                                 bool assume) {
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
    out << "soul ante=" << ante << " packs=" << 1 + (int)(rng() % 2)
        << " size=" << size;
    if (rng() % 3 == 0) out << " legendary=j_perkeo";
    break;
  }
  }
  return out.str();
}



static void testFast(int programs, int seedsPerProgram) {
  std::mt19937_64 rng(2026);
  long long checked = 0, wrong = 0, passes = 0, conditions = 0, gated = 0;
  for (int k = 0; k < programs; k++) {
    long long base = 1 + (long long)(rng() % 2318107019760ull);
    Seed baseSeed(base);
    // 1-6 conditions, mostly true for the base seed; several often share a
    // kind and ante (the case where the fast check shares work).
    int n = 1 + (int)(rng() % 6);
    bool assume = rng() % 3 == 0;
    std::string text = assume ? "assume enhanced\n" : "";
    for (int i = 0; i < n; i++) {
      std::string line = trueCondition(baseSeed, rng, assume);
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
      std::printf("    parse failed: %s\n%s", err.c_str(), text.c_str());
      failures++;
      continue;
    }
    conditions += prog->conditions().size();
    for (const Condition &c : prog->conditions())
      for (Item g : GATED) gated += c.item == g;
    for (int s = 0; s < seedsPerProgram; s++) {
      Seed seed(base + s); // the base seed itself, then its neighbours
      bool expect = true;
      for (const Condition &c : prog->conditions())
        expect = expect && prog->referenceCheck(c, seed);
      Seed again(base + s);
      bool got = prog->matches(again);
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
  for (Preset &p : presets) {
    std::string err;
    FilterConfig custom;
    custom.program = FilterProgram::parse(p.spec, err);
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
  testSearch();
  testPresets(quick ? 30000 : 300000);
  std::printf("\n%s (%d failing check%s)\n", failures ? "FAILED" : "ALL PASSED",
              failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
