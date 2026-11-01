// prefilter.hpp — a fast first check that runs on several seeds at once.
//
// WHY
//   Almost all of a search's time goes into producing random values, and
//   each new random "stream" starts by hashing its name one character at a
//   time: a chain of 64-bit divisions where every step waits for the one
//   before. A single seed therefore leaves most of the CPU idle. Most seeds
//   also fail the very first thing a filter checks (e.g. ~93% don't have a
//   Charm Tag), so that first check is where the time goes.
//
// WHAT
//   For a group of LANES consecutive seeds, the prefilter computes the
//   *first* ante 1 tag, voucher and/or shop pack the game would generate,
//   with the hash chains of all seeds interleaved so the CPU works on
//   several divisions at once. Seeds whose roll is definitely wrong are
//   rejected; everything else ("maybe") goes through the normal
//   brainstormFilter(), which checks everything again from scratch.
//
// WHY IT CANNOT CHANGE RESULTS
//   The prefilter only ever *rejects*, and only when the roll it computed is
//   exactly what the game produces (tests/prefilter_test.cpp compares every
//   computed roll with the normal engine). If the first pick is a locked
//   item, the game re-rolls with a new stream ("Tag1_resample2", then
//   "..._resample3", ...); the prefilter reproduces up to MAX_RESAMPLE of
//   those re-rolls, also batched. A seed still undecided after that is never
//   rejected; it just goes through the normal filter.
#pragma once

#include "filters.hpp"

#include <array>
#include <string>
#include <vector>

class Prefilter {
public:
  static const int LANES = 8;
  // Re-rolls ("_resample2" .. "_resample9") handled in the batch. Keeping
  // single-digit numbers keeps every key length fixed.
  static const int MAX_RESAMPLE = 9;

  // Which first rolls a FilterConfig lets us check. Only the plain menu
  // filters (no custom filter) are handled; everything else is inactive.
  explicit Prefilter(const FilterConfig &cfg);
  bool active() const { return !checks.empty(); }

  // For the n (1..LANES) seeds starting at `seed`, sets maybe[i] = false if
  // seed i certainly fails the filter, true otherwise. Advances `seed` by n.
  void run(Seed &seed, int n, bool maybe[LANES]);

  // One kind of first roll. Exposed for tests.
  enum Kind { TAG, VOUCHER, PACK };

  // Computes the first roll of `kind` for the n seeds starting at `seed`
  // (advancing it by n). decided[i] is false when the first pick was a
  // locked item, i.e. the game would re-roll and items[i] is not final.
  // Exposed for tests.
  void firstRolls(Kind kind, Seed &seed, int n, Item items[LANES],
                  bool decided[LANES]);

private:
  struct Check {
    Kind kind;
    Item want;
  };
  std::vector<Check> checks;
  // Items locked at the start of a run (Instance::initLocks(1, false, true)),
  // exactly what brainstormFilter() uses.
  std::vector<bool> locked;

  // Key lengths whose seed hash collect() must provide.
  std::vector<int> keyLengths;

  // Per-group scratch: seed hashes needed by the stream keys.
  struct LaneSeed {
    double hashed;     // seed.pseudohash(0)       = Instance::hashedSeed
    double prefix[24]; // seed.pseudohash(len)     for each len in keyLengths
  };
  void collect(Seed &seed, int n, LaneSeed lanes[LANES]);
  // First step of stream `key` for the given lanes: the value the game seeds
  // math.random with (Instance::get_node on a stream's first use).
  void streamStart(const std::string &key, const LaneSeed lanes[LANES],
                   const int *idx, int count, double out[LANES]);
  void roll(Kind kind, const LaneSeed lanes[LANES], const int *idx, int count,
            Item items[LANES], bool decided[LANES]);
};
