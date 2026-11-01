// scan.hpp — "find the first seed in this range that passes the filter".
//
// Used by the search engine's worker threads (brainstorm_api.cpp), the
// benchmark and the tests, so all of them exercise exactly the same code.
#pragma once

#include "filters.hpp"
#include "prefilter.hpp"

#include <atomic>
#include <cstdint>

// Tests the `count` consecutive seed IDs starting at `firstId` (the range
// must not wrap past "ZZZZZZZZ"), in order. Returns the ID of the first seed
// that passes brainstormFilter(), or -1 if none does or `stop` was raised.
//
// `prefilter` may be null. If it is active, seeds are first screened in
// groups (see prefilter.hpp); only possible matches run the full filter, so
// the answer is identical either way, just faster.
//
// `checked` is increased roughly every 4096 seeds (for progress display).
long long scanRange(const FilterConfig &cfg, Prefilter *prefilter,
                    long long firstId, long long count,
                    std::atomic<bool> &stop, std::atomic<uint64_t> &checked);
