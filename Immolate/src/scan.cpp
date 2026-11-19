// scan.cpp — see scan.hpp.

#include "scan.hpp"

#include "filterprog.hpp"
#include "functions.hpp"

#include <algorithm>

namespace {
const int PROGRESS_EVERY = 4096; // seeds between shared-counter updates
}

long long scanRange(const FilterConfig &cfg, Prefilter *prefilter,
                    long long firstId, long long count,
                    std::atomic<bool> &stop, std::atomic<uint64_t> &checked) {
  int sinceReport = 0;
  auto report = [&](int n) {
    sinceReport += n;
    if (sinceReport >= PROGRESS_EVERY) {
      checked.fetch_add(sinceReport, std::memory_order_relaxed);
      sinceReport = 0;
    }
  };

  // With only a custom filter set (what the mod sends), the filter program
  // is asked directly: it needs no locks and no copy of the Instance, which
  // brainstormFilter() makes for the menu options.
  const FilterProgram *only = cfg.programOnly() ? cfg.program.get() : nullptr;
  auto passes = [&](Instance &inst) {
    return only ? only->matches(inst) : brainstormFilter(inst, cfg) >= 1;
  };

  if (!prefilter || !prefilter->active()) {
    // Plain path: every seed through the full filter.
    Seed s(firstId);
    Instance inst(s);
    for (long long i = 0; i < count; i++) {
      if (stop.load(std::memory_order_relaxed)) break;
      if (passes(inst)) {
        checked.fetch_add(sinceReport, std::memory_order_relaxed);
        return firstId + i;
      }
      report(1);
      inst.next();
    }
    checked.fetch_add(sinceReport, std::memory_order_relaxed);
    return -1;
  }

  // Prefiltered path: screen LANES seeds at a time, then run the full
  // filter on the possible matches, in seed order, so the first match found
  // is the same one the plain path would find.
  Seed s(firstId);
  bool maybe[Prefilter::LANES];
  for (long long done = 0; done < count;) {
    if (stop.load(std::memory_order_relaxed)) break;
    int n = (int)std::min<long long>(Prefilter::LANES, count - done);
    prefilter->run(s, n, maybe); // advances s by n
    for (int i = 0; i < n; i++) {
      if (!maybe[i]) continue;
      long long id = firstId + done + i;
      Seed one(id);
      Instance inst(one);
      if (passes(inst)) {
        checked.fetch_add(sinceReport + i, std::memory_order_relaxed);
        return id;
      }
    }
    done += n;
    report(n);
  }
  checked.fetch_add(sinceReport, std::memory_order_relaxed);
  return -1;
}
