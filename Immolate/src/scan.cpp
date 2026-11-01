// scan.cpp — see scan.hpp.

#include "scan.hpp"

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

  if (!prefilter || !prefilter->active()) {
    // Plain path: every seed through the full filter.
    Seed s(firstId);
    Instance inst(s);
    for (long long i = 0; i < count; i++) {
      if (stop.load(std::memory_order_relaxed)) break;
      if (brainstormFilter(inst, cfg) >= 1) {
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
      if (brainstormFilter(inst, cfg) >= 1) {
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
