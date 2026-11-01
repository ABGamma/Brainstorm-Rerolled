/*
 * brainstorm_api.h — the C interface between the Brainstorm Lua mod and the
 * native Immolate seed searcher.
 *
 * WHY A PLAIN C INTERFACE?
 *   The mod calls this library through LuaJIT's FFI, which can only call
 *   plain C functions using plain C types. C++ types (std::string, classes)
 *   and C++ name mangling differ between compilers and operating systems, so
 *   everything crossing this boundary is a C type: ints, doubles, pointers
 *   and NUL-terminated char strings. The same header therefore works for the
 *   Windows .dll and the macOS .dylib.
 *
 *   brainstorm/Core/search.lua contains a copy of the declarations below
 *   (inside ffi.cdef). If you change anything here, change it there too and
 *   bump BS_API_VERSION so a mismatched library/mod pair is detected.
 *
 * HOW A SEARCH WORKS (asynchronous, so the game never freezes)
 *
 *     job = bs_search_start(&params)      -- returns immediately; worker
 *                                            threads begin searching
 *     every frame:
 *         status = bs_search_poll(job, seed_buf, &seeds_checked)
 *         if status == BS_FOUND  -> seed_buf holds the seed, e.g. "7NTPKW6P"
 *         if status == BS_RUNNING -> keep waiting, show seeds_checked
 *     bs_search_cancel(job)               -- optional: ask workers to stop
 *     bs_search_free(job)                 -- ALWAYS call once you are done;
 *                                            waits for threads, frees memory
 *
 * THREAD SAFETY
 *   One job may be polled from one thread (the game's main thread). Several
 *   jobs may exist at once, though the mod only ever runs one.
 */
#ifndef BRAINSTORM_API_H
#define BRAINSTORM_API_H

#include <stdint.h>

#if defined(_WIN32)
#define BS_API __declspec(dllexport)
#else
#define BS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Increment whenever a struct layout or function signature changes.
 * Version history:
 *   1  first version
 *   2  bs_params.filter_spec added at the end. The library still accepts
 *      api_version 1 (and then never reads filter_spec), so an older mod
 *      keeps working with a newer library. */
#define BS_API_VERSION 2

/* Return values of bs_search_poll(). */
enum {
  BS_RUNNING = 0,   /* still searching                                  */
  BS_FOUND = 1,     /* a matching seed was written to the output buffer */
  BS_EXHAUSTED = 2, /* every seed was checked and none matched          */
  BS_CANCELLED = 3, /* bs_search_cancel() was called                    */
  BS_ERROR = 4      /* bad parameters; see bs_last_error()              */
};

/*
 * Everything the user selected in the Brainstorm settings menu.
 *
 * Item fields (tag, voucher, pack) accept the game's internal key, which
 * does not change with the game language:
 *     tag      "tag_charm", "tag_double", ...
 *     voucher  "v_telescope", "v_overstock_norm", ...
 *     pack     "p_arcana_mega", "p_buffoon_normal", ...  (no trailing _1/_2)
 * An empty string or NULL means "don't filter on this".
 * For backwards compatibility the English display name ("Charm Tag") is also
 * accepted.
 *
 * Boolean fields use int32_t (0 = off, 1 = on) because the size of C `bool`
 * across an FFI boundary is a classic source of subtle bugs.
 */
typedef struct bs_params {
  int32_t api_version; /* must be BS_API_VERSION                          */

  const char *start_seed; /* where to begin searching, 1-8 chars of
                             1-9/A-Z; NULL or "" starts at seed "1"      */
  int32_t threads;        /* CPU worker threads; 0 = automatic           */

  const char *tag;
  const char *voucher;
  const char *pack;
  int32_t souls; /* number of The Soul cards required in Charm Tag
                    Mega Arcana packs (0 = don't care)                 */

  int32_t observatory; /* Telescope ante 1 + Observatory ante 2         */
  int32_t perkeo;      /* The Soul must give Perkeo (needs souls >= 1)  */
  int32_t copy_money;  /* "Early copy + money"                          */
  int32_t retcon;      /* Retcon voucher by ante 8                      */
  int32_t bean;        /* Turtle Bean in the first 150 ante 8 shop items */
  int32_t burglar;     /* Burglar in the ante 6/7/8 shops               */

  const char *custom_filter; /* Legacy named filters; none exist any more.
                                Ignored; pass "No Filter" or "".        */

  /* Since version 2. A custom filter built in game: conditions in the text
   * format described in filterprog.hpp, e.g.
   *     "joker j_blueprint ante=1 within=4 edition=e_negative"
   * Checked in addition to the options above. NULL or "" = none. A mistake
   * makes bs_search_start() fail with a message in bs_last_error(). */
  const char *filter_spec;
} bs_params;

/* Opaque handle to a running search. Lua only ever holds the pointer. */
typedef struct bs_job bs_job;

/* Returns BS_API_VERSION of the compiled library. */
BS_API int32_t bs_api_version(void);

/* Human-readable build info, e.g. "Immolate 2.0.0 (cpu, 15 threads)". */
BS_API const char *bs_build_info(void);

/*
 * Starts a search in background threads and returns immediately.
 * Returns NULL if the parameters are invalid (bs_last_error() says why).
 * The params struct and its strings are copied; they may be freed after
 * this call returns.
 */
BS_API bs_job *bs_search_start(const bs_params *params);

/*
 * Non-blocking status check. Cheap enough to call every frame.
 *   seed_out       optional, at least 9 bytes; receives the found seed
 *   seeds_checked  optional; receives how many seeds have been tested
 */
BS_API int32_t bs_search_poll(bs_job *job, char *seed_out,
                              uint64_t *seeds_checked);

/* Asks the workers to stop. Returns immediately; poll reports CANCELLED. */
BS_API void bs_search_cancel(bs_job *job);

/* Cancels if needed, waits for all worker threads to exit, frees the job.
 * The pointer must not be used afterwards. Safe to call with NULL. */
BS_API void bs_search_free(bs_job *job);

/* Message describing the most recent error on the calling thread. */
BS_API const char *bs_last_error(void);

/*
 * Checks a single seed against the parameters without starting threads.
 * Returns 1 if it matches, 0 if not, -1 on bad parameters. Used by tests and
 * handy for a "does my current seed pass?" button.
 */
BS_API int32_t bs_check_seed(const bs_params *params, const char *seed);

#ifdef __cplusplus
}
#endif

#endif /* BRAINSTORM_API_H */
