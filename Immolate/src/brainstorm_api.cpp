// brainstorm_api.cpp — implementation of the C API in brainstorm_api.h.
//
// Overview of a search job:
//
//   bs_search_start() validates the parameters, builds a FilterConfig and
//   launches N worker threads, then returns to Lua immediately.
//
//   Each worker repeatedly claims the next unclaimed "block" of seeds
//   (BLOCK_SIZE consecutive seed IDs) from a shared atomic counter, and runs
//   brainstormFilter() on every seed in it. Claiming blocks dynamically means
//   fast threads simply do more blocks; nobody waits for a slow thread.
//
//   The first worker to find a match records it and raises `stop`; the
//   others notice within one seed and exit. bs_search_poll() just reads these
//   shared variables, so it never blocks the game.
//
// Seed IDs: Immolate numbers every seed from 1 ("1") to SEED_ID_MAX
// ("ZZZZZZZZ"), and Seed::next() always moves to ID + 1. A search starting at
// ID s visits s, s+1, ..., SEED_ID_MAX, 1, 2, ..., s-1 — every seed exactly
// once.

#include "brainstorm_api.h"
#include "filterprog.hpp"
#include "filters.hpp"
#include "functions.hpp"
#include "prefilter.hpp"
#include "scan.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef IMMOLATE_VERSION
#define IMMOLATE_VERSION "dev"
#endif

namespace {

const long long SEED_ID_MAX = 2318107019760LL; // ID of "ZZZZZZZZ"
const long long BLOCK_SIZE = 1 << 20;          // ~1M seeds per claimed block

thread_local std::string g_lastError;

bool setError(const std::string &msg) {
  g_lastError = msg;
  return false;
}

// Returns the seed ID `offset` steps after `startId`, wrapping past
// SEED_ID_MAX back to 1.
long long wrapId(long long startId, long long offset) {
  return (startId - 1 + offset) % SEED_ID_MAX + 1;
}

bool validSeedString(const char *s) {
  size_t n = std::strlen(s);
  if (n < 1 || n > 8) return false;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 128 || charSeeds[c] < 0) return false;
  }
  return true;
}

// Reads a C string field, treating NULL as "".
std::string str(const char *s) { return s ? std::string(s) : std::string(); }

// Translates the C parameter struct into a FilterConfig.
bool buildConfig(const bs_params *p, FilterConfig &cfg) {
  if (!p) return setError("params is NULL");
  if (p->api_version < 1 || p->api_version > BS_API_VERSION)
    return setError("API version mismatch: mod sent " +
                    std::to_string(p->api_version) +
                    ", library supports 1 to " +
                    std::to_string(BS_API_VERSION));

  bool ok = true;
  cfg.tag = itemFromKey(str(p->tag), ok);
  if (!ok) return setError("unknown tag '" + str(p->tag) + "'");
  cfg.voucher = itemFromKey(str(p->voucher), ok);
  if (!ok) return setError("unknown voucher '" + str(p->voucher) + "'");
  cfg.pack = itemFromKey(str(p->pack), ok);
  if (!ok) return setError("unknown pack '" + str(p->pack) + "'");

  cfg.souls = std::max(0, (int)p->souls);
  cfg.observatory = p->observatory != 0;
  cfg.perkeo = p->perkeo != 0;
  cfg.copymoney = p->copy_money != 0;
  cfg.retcon = p->retcon != 0;
  cfg.bean = p->bean != 0;
  cfg.burglar = p->burglar != 0;
  cfg.custom = stringToFilter(str(p->custom_filter));

  // filter_spec only exists in version 2+ structs: never read it otherwise.
  if (p->api_version >= 2 && p->filter_spec && *p->filter_spec) {
    std::string error;
    std::unique_ptr<FilterProgram> prog =
        FilterProgram::parse(p->filter_spec, error);
    if (!prog) return setError("custom filter, " + error);
    if (!prog->conditions().empty()) cfg.program = std::move(prog);
  }
  return true;
}

int autoThreadCount() {
  // Leave one core free so the game itself stays smooth while we search.
  unsigned hw = std::thread::hardware_concurrency();
  return hw > 1 ? (int)hw - 1 : 1;
}

} // namespace

// ---------------------------------------------------------------------------
// Key -> Item lookup (declared in filters.hpp)

namespace {
struct KeyItem {
  const char *key;
  Item item;
};

// Keys exactly as they appear in Balatro's game files (G.P_TAGS,
// G.P_CENTERS). These never change with the game language.
const KeyItem KEY_TABLE[] = {
    // Tags
    {"tag_uncommon", Item::Uncommon_Tag},
    {"tag_rare", Item::Rare_Tag},
    {"tag_negative", Item::Negative_Tag},
    {"tag_foil", Item::Foil_Tag},
    {"tag_holo", Item::Holographic_Tag},
    {"tag_polychrome", Item::Polychrome_Tag},
    {"tag_investment", Item::Investment_Tag},
    {"tag_voucher", Item::Voucher_Tag},
    {"tag_boss", Item::Boss_Tag},
    {"tag_standard", Item::Standard_Tag},
    {"tag_charm", Item::Charm_Tag},
    {"tag_meteor", Item::Meteor_Tag},
    {"tag_buffoon", Item::Buffoon_Tag},
    {"tag_handy", Item::Handy_Tag},
    {"tag_garbage", Item::Garbage_Tag},
    {"tag_ethereal", Item::Ethereal_Tag},
    {"tag_coupon", Item::Coupon_Tag},
    {"tag_double", Item::Double_Tag},
    {"tag_juggle", Item::Juggle_Tag},
    {"tag_d_six", Item::D6_Tag},
    {"tag_top_up", Item::Top_up_Tag},
    {"tag_skip", Item::Speed_Tag},
    {"tag_orbital", Item::Orbital_Tag},
    {"tag_economy", Item::Economy_Tag},
    // Vouchers
    {"v_overstock_norm", Item::Overstock},
    {"v_overstock_plus", Item::Overstock_Plus},
    {"v_clearance_sale", Item::Clearance_Sale},
    {"v_liquidation", Item::Liquidation},
    {"v_hone", Item::Hone},
    {"v_glow_up", Item::Glow_Up},
    {"v_reroll_surplus", Item::Reroll_Surplus},
    {"v_reroll_glut", Item::Reroll_Glut},
    {"v_crystal_ball", Item::Crystal_Ball},
    {"v_omen_globe", Item::Omen_Globe},
    {"v_telescope", Item::Telescope},
    {"v_observatory", Item::Observatory},
    {"v_grabber", Item::Grabber},
    {"v_nacho_tong", Item::Nacho_Tong},
    {"v_wasteful", Item::Wasteful},
    {"v_recyclomancy", Item::Recyclomancy},
    {"v_tarot_merchant", Item::Tarot_Merchant},
    {"v_tarot_tycoon", Item::Tarot_Tycoon},
    {"v_planet_merchant", Item::Planet_Merchant},
    {"v_planet_tycoon", Item::Planet_Tycoon},
    {"v_seed_money", Item::Seed_Money},
    {"v_money_tree", Item::Money_Tree},
    {"v_blank", Item::Blank},
    {"v_antimatter", Item::Antimatter},
    {"v_magic_trick", Item::Magic_Trick},
    {"v_illusion", Item::Illusion},
    {"v_hieroglyph", Item::Hieroglyph},
    {"v_petroglyph", Item::Petroglyph},
    {"v_directors_cut", Item::Directors_Cut},
    {"v_retcon", Item::Retcon},
    {"v_paint_brush", Item::Paint_Brush},
    {"v_palette", Item::Palette},
    // Booster packs (the game key minus its trailing "_1", "_2", ...)
    {"p_arcana_normal", Item::Arcana_Pack},
    {"p_arcana_jumbo", Item::Jumbo_Arcana_Pack},
    {"p_arcana_mega", Item::Mega_Arcana_Pack},
    {"p_celestial_normal", Item::Celestial_Pack},
    {"p_celestial_jumbo", Item::Jumbo_Celestial_Pack},
    {"p_celestial_mega", Item::Mega_Celestial_Pack},
    {"p_standard_normal", Item::Standard_Pack},
    {"p_standard_jumbo", Item::Jumbo_Standard_Pack},
    {"p_standard_mega", Item::Mega_Standard_Pack},
    {"p_buffoon_normal", Item::Buffoon_Pack},
    {"p_buffoon_jumbo", Item::Jumbo_Buffoon_Pack},
    {"p_buffoon_mega", Item::Mega_Buffoon_Pack},
    {"p_spectral_normal", Item::Spectral_Pack},
    {"p_spectral_jumbo", Item::Jumbo_Spectral_Pack},
    {"p_spectral_mega", Item::Mega_Spectral_Pack},
};
} // namespace

Item itemFromKey(const std::string &key, bool &ok) {
  ok = true;
  if (key.empty()) return Item::RETRY;
  for (const KeyItem &k : KEY_TABLE)
    if (key == k.key) return k.item;
  // Legacy: English display names, as the old mod sent them.
  Item legacy = stringToItem(key);
  if (legacy != Item::RETRY) return legacy;
  ok = false;
  return Item::RETRY;
}

std::string keyFromItem(Item item) {
  for (const KeyItem &k : KEY_TABLE)
    if (k.item == item) return k.key;
  return "";
}

// ---------------------------------------------------------------------------
// The job object

struct bs_job {
  FilterConfig cfg;
  long long startId = 1;

  std::atomic<long long> nextBlock{0};  // next block index to claim
  std::atomic<uint64_t> checked{0};     // seeds tested so far (approximate)
  std::atomic<bool> stop{false};        // tells workers to exit
  std::atomic<bool> cancelled{false};
  std::atomic<bool> found{false};
  std::atomic<int> activeWorkers{0};

  std::mutex resultMutex;
  char result[9] = {0};

  std::vector<std::thread> threads;

  // Shared by all workers (read-only while searching). See prefilter.hpp.
  std::unique_ptr<Prefilter> prefilter;

  // Tests `count` consecutive seeds starting at `firstId` (the range must not
  // wrap past SEED_ID_MAX; the caller splits it) and records any match.
  void scanBlock(long long firstId, long long count) {
    long long id = ::scanRange(cfg, prefilter.get(), firstId, count, stop,
                               checked);
    if (id < 0) return;
    // Write the seed BEFORE publishing `found`, so a poll that sees
    // found == true is guaranteed to read a complete string.
    std::lock_guard<std::mutex> lock(resultMutex);
    if (!found) {
      std::string str = Seed(id).tostring();
      std::strncpy(result, str.c_str(), sizeof(result) - 1);
      found = true;
    }
    stop = true;
  }

  void worker() {
    const long long totalBlocks = (SEED_ID_MAX + BLOCK_SIZE - 1) / BLOCK_SIZE;
    while (!stop.load(std::memory_order_relaxed)) {
      long long block = nextBlock.fetch_add(1);
      if (block >= totalBlocks) break;
      long long offset = block * BLOCK_SIZE;
      long long count = std::min(BLOCK_SIZE, SEED_ID_MAX - offset);
      long long first = wrapId(startId, offset);
      // Split the block in two if it runs past "ZZZZZZZZ".
      long long untilWrap = SEED_ID_MAX - first + 1;
      if (count <= untilWrap) {
        scanBlock(first, count);
      } else {
        scanBlock(first, untilWrap);
        scanBlock(1, count - untilWrap);
      }
    }
    activeWorkers.fetch_sub(1);
  }
};

// ---------------------------------------------------------------------------
// Exported functions

extern "C" {

BS_API int32_t bs_api_version(void) { return BS_API_VERSION; }

BS_API const char *bs_build_info(void) {
  static std::string info = std::string("Immolate ") + IMMOLATE_VERSION +
                            " (cpu, " + std::to_string(autoThreadCount()) +
                            " threads)";
  return info.c_str();
}

BS_API const char *bs_last_error(void) { return g_lastError.c_str(); }

BS_API bs_job *bs_search_start(const bs_params *params) {
  FilterConfig cfg;
  if (!buildConfig(params, cfg)) return nullptr;

  long long startId = 1;
  if (params->start_seed && params->start_seed[0]) {
    if (!validSeedString(params->start_seed)) {
      setError(std::string("invalid start seed '") + params->start_seed +
               "' (use 1-8 characters from 1-9 and A-Z)");
      return nullptr;
    }
    startId = Seed(std::string(params->start_seed)).getID();
  }

  int threads = params->threads > 0 ? params->threads : autoThreadCount();

  bs_job *job = new bs_job();
  job->cfg = cfg;
  job->prefilter.reset(new Prefilter(cfg));
  job->startId = startId;
  job->activeWorkers = threads;
  for (int t = 0; t < threads; t++)
    job->threads.emplace_back([job] { job->worker(); });
  return job;
}

BS_API int32_t bs_search_poll(bs_job *job, char *seed_out,
                              uint64_t *seeds_checked) {
  if (!job) return BS_ERROR;
  // Read the worker count BEFORE `found`. A worker sets `found` before it
  // exits, so if we see zero workers here, any match they made is already
  // visible below. (Reading in the other order could report EXHAUSTED for a
  // search that found its seed in between the two reads.)
  int active = job->activeWorkers.load();
  if (seeds_checked) *seeds_checked = job->checked.load();
  if (job->found) {
    if (seed_out) {
      std::lock_guard<std::mutex> lock(job->resultMutex);
      std::memcpy(seed_out, job->result, sizeof(job->result));
    }
    return BS_FOUND;
  }
  if (job->cancelled) return BS_CANCELLED;
  if (active == 0) return BS_EXHAUSTED;
  return BS_RUNNING;
}

BS_API void bs_search_cancel(bs_job *job) {
  if (!job) return;
  job->cancelled = true;
  job->stop = true;
}

BS_API void bs_search_free(bs_job *job) {
  if (!job) return;
  job->stop = true;
  for (auto &t : job->threads) t.join();
  delete job;
}

BS_API int32_t bs_check_seed(const bs_params *params, const char *seed) {
  FilterConfig cfg;
  if (!buildConfig(params, cfg)) return -1;
  if (!seed || !validSeedString(seed)) {
    setError("invalid seed");
    return -1;
  }
  Seed s{std::string(seed)};
  Instance inst(s);
  return brainstormFilter(inst, cfg) >= 1 ? 1 : 0;
}

} // extern "C"
