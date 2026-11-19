# How Brainstorm Rerolled works

This document walks through the whole system, from pressing **Ctrl+A** (the
default auto-reroll key; every key can be changed in the Hotkeys sub-tab) to
a new run starting on a matching seed. Read it top to bottom once; afterwards
the section headings work as a reference.

- [1. The big picture](#1-the-big-picture)
- [2. Why seeds can be predicted at all](#2-why-seeds-can-be-predicted-at-all)
- [3. The Lua side](#3-the-lua-side)
- [4. The C API — the contract between Lua and C++](#4-the-c-api--the-contract-between-lua-and-c)
- [5. The search engine (threads)](#5-the-search-engine-threads)
- [6. Filters](#6-filters)
- [7. Floating-point exactness](#7-floating-point-exactness)
- [7b. Performance](#7b-performance)
- [8. Building for each platform](#8-building-for-each-platform)
- [9. Tests](#9-tests)
- [10. File map](#10-file-map)

---

## 1. The big picture

```
 ┌──────────────────────────── Balatro (LuaJIT) ────────────────────────────┐
 │                                                                          │
 │  Ctrl+A ──► Brainstorm.startAutoReroll()        (Core/Brainstorm.lua)    │
 │                 │ reads the settings menu                                │
 │                 ▼                                                        │
 │             native.start(opts) ─────────────────┐  (Core/native.lua)     │
 │                                                 │  LuaJIT FFI call       │
 │  every frame:                                   │                        │
 │  Game:update ──► native.poll(job) ──────────┐   │                        │
 │                 "running" → update text     │   │                        │
 │                 "found"   → start the run   │   │                        │
 └─────────────────────────────────────────────┼───┼────────────────────────┘
                                               ▼   ▼
 ┌──────────────── Immolate.dll / Immolate.dylib (C++) ─────────────────────┐
 │  bs_search_poll()   bs_search_start()          (src/brainstorm_api.cpp)  │
 │        ▲                  │ launches N worker threads                    │
 │        │ reads shared     ▼                                              │
 │        │ atomics     ┌─────────┐ ┌─────────┐ ┌─────────┐                 │
 │        └─────────────│worker 1 │ │worker 2 │ │worker N │ …               │
 │                      └────┬────┘ └────┬────┘ └────┬────┘                 │
 │                           ▼ for each seed                                │
 │                  brainstormFilter(seed, cfg)          (src/filters.cpp)  │
 │                           ▼ asks "what would the game generate?"         │
 │                  Instance::nextTag / nextPack / …   (src/functions.hpp)  │
 │                           ▼                                              │
 │                  pseudohash + LuaRandom     (src/util.cpp, seed.cpp)     │
 └──────────────────────────────────────────────────────────────────────────┘
```

The key idea: **the search runs on background threads**. The Lua side only
starts it and then asks "done yet?" once per frame, a call that takes
microseconds. That is why the game no longer freezes. (The old design made
one call that only returned once a seed was found, so the game could not
draw a single frame in the meantime.)

---

## 2. Why seeds can be predicted at all

Balatro does not use "true" randomness. Every random decision is computed from
the **seed string** plus a **key** naming the decision, for example
`"Tag1"` for the ante 1 skip tag, or `"shop_pack1"` for the ante 1 booster
packs. The game's Lua code does roughly:

```lua
-- simplified from Balatro's functions/misc_functions.lua
function pseudoseed(key)
  -- each key has its own running value, advanced every time it is used
  G.GAME.pseudorandom[key] = advance(G.GAME.pseudorandom[key]
                                     or pseudohash(key .. seed))
  return (G.GAME.pseudorandom[key] + G.GAME.pseudorandom.hashed_seed) / 2
end
-- ...that number seeds LuaJIT's random generator, which picks the item.
```

Immolate re-implements exactly this in C++:

| Game (Lua)                        | Immolate (C++)                                   |
|-----------------------------------|--------------------------------------------------|
| `pseudohash(str)`                 | `pseudohash()` in `util.cpp`                     |
| `G.GAME.pseudorandom[key]` state  | `Instance::get_node(key)` + `cache.nodes`        |
| `math.randomseed` + `math.random` | `LuaRandom` in `util.cpp` (LuaJIT's xoshiro-like PRNG, bit-for-bit) |
| `pseudorandom_element(pool, …)`   | `Instance::randchoice()` in `instance.hpp`       |
| "is this item unlocked?"          | `Instance::locked[]`, `initLocks()`              |

Because every step is reproduced exactly, Immolate can answer "what tag will
seed `7NTPKW6P` offer in ante 1?" **without running the game**, millions of
times per second.

**Order matters.** Calling `inst.nextTag(1)` twice gives the *second* tag the
`"Tag1"` stream produces, just like the game. Each filter therefore gets a
**fresh copy** of the `Instance` for every seed (`brainstormFilter(Instance
inst, …)` takes it *by value*), so one seed's queries never leak into the
next.

**Seed IDs.** Seeds are 1–8 characters from `123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ`
(35 symbols: every digit except 0, and every letter). Immolate numbers all 2,318,107,019,760 of them, from
ID 1 (`"1"`) to the last one (`"ZZZZZZZZ"`), and `Seed::next()` steps to
ID + 1. It also caches partial hashes so that stepping to the next seed only
re-hashes the characters that changed (see the comment in `seed.hpp`).

---

## 3. The Lua side

### `Brainstorm/lovely.toml`
Lovely (the mod loader) appends `Core/Brainstorm.lua` to the game's `main.lua`
and calls `Brainstorm.init()` once the game has loaded its profile.

### `Brainstorm/Core/Brainstorm.lua`
- `Brainstorm.init()` finds the mod folder, loads the config and the settings
  UI, then loads `Core/native.lua` and asks it to load the native library. If
  that fails (e.g. a missing file), it prints why and the rest of the mod
  (save states, fast reroll) keeps working.
- `key_press_update` (hooked) handles the hotkeys. All of them are stored in
  `Brainstorm.config.keybinds` and are pressed together with the modifier
  key (`modifier`, default left Ctrl). Defaults: **A** auto-reroll, **R**
  fast reroll, **T** open the Brainstorm-Rerolled settings tab, **S** save the
  current seed, **Z** / **X** save / load a save state (with a number 1-5 for
  the slot). While the Hotkeys sub-tab waits for a new key
  (`Brainstorm.ST.capturing`), every key press goes to `ST.capture(key)`
  instead.
- The auto-reroll key calls `startAutoReroll()` or, when a search is running,
  `stopAutoReroll("Search cancelled")`.
- `searchOptions()` turns the settings menu into a plain table:
  `{ tag = "tag_charm", voucher = "v_telescope", pack = "p_arcana_mega", souls = 1, ... }`.
  Note these are the game's **internal keys**, which are the same in every
  language. The old code sent translated names like "Charm Tag", which broke
  for non-English players.
- `Game:update` (hooked) calls `pollAutoReroll()` every frame while a search
  is running. That updates `Brainstorm.ar_status` ("Searching… 12.3M seeds
  (8.1M/s)"). The on-screen text object reads that variable each frame through
  `ref_table`/`ref_value`, which is Balatro's standard way to display a live
  value.
- On `"found"`, `startFoundRun(seed)` restarts the game on that seed, exactly
  as before.

### `Brainstorm/Core/native.lua`
The only file that touches the FFI.
1. `ffi.cdef[[ ... ]]` declares the C structs and functions. It is a copy of
   `brainstorm_api.h`, and **the two must match exactly**. A mismatch is
   detected at load time through `bs_api_version()`.
2. `native.load(path)` picks `Immolate.dll` on Windows (including Proton) or
   `Immolate.dylib` on macOS, based on LuaJIT's `ffi.os`. Android's LuaJIT
   says "Linux" too, so Android is recognised with `love.system.getOS()`
   and loads `Immolate-android-arm64.so` (64-bit ARM only). Should Android
   refuse to load it where it is, it is copied to LÖVE's save folder (the
   game's private folder) and loaded from there. With Lovely Mobile Maker
   builds the Mods folder is already inside the game's private folder, and
   the library loads where it is.
3. `native.start/poll/cancel/free` are thin wrappers. `start` attaches
   `ffi.gc(job, bs_search_free)` so that even a forgotten job is cleaned up
   when Lua garbage-collects it; `free` removes that hook first so the job is
   never freed twice.

### `Brainstorm/UI/settings.lua` (the settings tab)
Settings > Brainstorm-Rerolled (added by `UI/ui.lua`) shows `Brainstorm.ST.tab()`:
a row of four sub-tab buttons over a box of fixed size, so nothing shifts
when switching. `ST.show(sub)` swaps the box's contents in place (the object
in the `bs_st_body` node), the same way `FB.go` swaps filter screens.
- **Filters**: the saved filter list from `FB.list_rows()`.
- **Saved seeds**: `Brainstorm.config.saved_seeds = { list = { {seed, note},
  ... }, deck, stake }`, newest first, at most 100, 5 per page.
  `ST.save_current_seed()` (hotkey or button, only during a run) adds the
  current seed unless it is already saved. Each seed has a note text box.
  Balatro draws only one text box per UIBox (each takes `draw_layer` 1 and 2
  and a UIBox keeps one node per layer), so every note box is wrapped in its
  own UIBox. The config is written when typing ends. `ST.play(i)` starts the
  seed with the deck and stake chosen in the two cyclers above the list.
  These are shared by all seeds, not stored per seed. During a run, Play asks
  "Sure?" first.
- **Hotkeys**: one button per key in `Brainstorm.config.keybinds`. Clicking
  one sets `ST.capturing`; the next key press is stored unless it is 1-5
  (used for save-state slots) or already taken. Esc cancels. "Reset to
  defaults" restores `ST.DEFAULT_KEYS`.

- **Mobile**: the Mobile Friendly toggle (`UI/mobile.lua`, below).

`ST.ensure_config()` fills in anything missing from older configs at start.

### `Brainstorm/UI/mobile.lua` (touch controls)
Every action has a hotkey, which a phone can't press. With
`Brainstorm.config.mobile` on (the "Mobile Friendly" toggle; on for Android
and off elsewhere at the first start), the Mobile part of the settings tab
has buttons for Search / Stop search, Reroll, Save this seed, and Save /
Load for the five save-state slots. Load asks "Sure?" during a run. The
buttons call the same functions as the hotkeys (`Brainstorm.toggleAutoReroll`,
`Brainstorm.reroll`, `Brainstorm.saveState(k)`, `Brainstorm.loadState(k)`,
`ST.save_current_seed`) and close the menu first, so the result can be seen.

During a run a "Brainstorm" button at the middle of the right screen edge
(between the consumable slots and the deck, where the game puts nothing)
opens the Mobile part. A hook on `Game:start_run` creates it
(`MB.update_run_button`). It needs no removing: everything created during
a run is removed by the game when the run ends (`Game:delete_run`).

---

## 4. The C API — the contract between Lua and C++

Defined in `Immolate/src/brainstorm_api.h`, which is commented in detail.

Why a C API instead of calling C++ directly? LuaJIT's FFI only understands C.
C++ features like `std::string`, classes, and *name mangling* (the compiler
renaming `brainstorm_cpp(std::string, …)` to something like
`?brainstorm_cpp@@YA?AV?$basic_string…`) differ between compilers, so none of
them can cross the boundary. Everything that crosses is an `int32_t`, a
`double`, a pointer, or a `const char*`.

Design choices worth knowing:

| Choice | Reason |
|---|---|
| Parameters in one `bs_params` struct, with `api_version` as its first field | Adding a filter option later means adding a struct field and bumping the version, not changing a function with 12 positional arguments. |
| `int32_t` instead of `bool` | The size of `bool` across FFI/compilers is a classic source of silent bugs. |
| An opaque `bs_job*` handle | Lua never sees inside the job, so the C++ side can change freely. |
| Strings are copied in `bs_search_start` | Lua may garbage-collect its strings right after the call. |
| `bs_last_error()` | Errors arrive in Lua as readable text ("unknown voucher 'v_bogus'"). |

The old blocking `brainstorm(...)` function is still exported (`immolate.cpp`),
implemented on top of the new API, for anyone with older scripts.

---

## 5. The search engine (threads)

`src/brainstorm_api.cpp`, struct `bs_job`.

- **Work splitting.** The 2.3 trillion seed IDs are cut into blocks of about
  1M. Each worker repeatedly does `block = nextBlock.fetch_add(1)` (an atomic
  "take a ticket" operation, so two threads can never get the same block) and
  scans it. Fast threads simply take more blocks.
- **Where it starts.** At the random `start_seed` that Lua generates, so
  repeated searches give different results. The search wraps from
  `ZZZZZZZZ` back to `1`, so every seed is visited exactly once. (The old code
  did not wrap and could run off the end into invalid seeds.)
- **Stopping.** The first worker to find a match writes the seed, then sets
  `found = true` and `stop = true`. Every worker checks `stop` before each seed,
  so all of them exit within microseconds. `bs_search_cancel` just sets
  `stop` too.
- **Progress.** Each worker counts locally and adds to the shared `checked`
  counter every 4096 seeds. Updating a shared counter on *every* seed (as the
  old code did) makes all cores fight over one cache line and slows the
  search down.
- **Thread count.** By default all cores minus one, so the game itself stays
  smooth.

Two subtle ordering rules, each with a comment in the code. Both were real
bugs found by the tests during development:
1. The seed string is written **before** `found` is set. Otherwise a poll
   could see `found` and read a half-written string.
2. `bs_search_poll` reads the worker count **before** checking `found`.
   Otherwise a worker could find a seed and exit between the two reads, and
   poll would wrongly report "exhausted".

---

## 6. Filters

`src/filters.hpp` / `src/filters.cpp`.

`brainstormFilter(Instance inst, const FilterConfig &cfg)` returns `0`
(reject) or `>= 1` (accept). `FilterConfig` holds the user's choices,
translated once from the strings Lua sent (`buildConfig()` in
`brainstorm_api.cpp`, using the key table `KEY_TABLE`).

The old code kept these settings in global variables (`BRAINSTORM_TAG`, …).
That worked for one search at a time but is fragile with threads, and made
the filter impossible to test in isolation. The filter *logic* itself was
moved over unchanged, and the golden test proves it gives the same answers.

### Game data: the lists a search predicts from (`src/gamedata.hpp/.cpp`)
Balatro picks "item number N of an ordered list", N coming from the seed. The
random numbers are the same in every copy of the game; the lists are not.
Mods add jokers, tags, vouchers, packs, rarities and editions, and a deck or
challenge changes what is available. So custom filters do not read the item
tables built into Immolate. They read a `GameData`:

- `GameData::vanilla()`: the unmodded game on a plain deck, built from
  `items.hpp`. Used when the mod sends nothing, and by the tests.
- `GameData::parse(text)`: the lists of the running game. The mod produces
  the text (`Brainstorm/Core/gamedata.lua`) and sends it with every search
  in `bs_params.game_data` (API version 3). `gamedata.hpp` documents the
  format.

How the mod gets the lists without knowing what any mod added: it calls the
game's own functions the way the game does. `get_current_pool()` gives the
tag, voucher and joker lists. For the things decided by one random roll (which
pack, which joker rarity, which edition), the roll is replaced by a number of
our choosing and the answer recorded for rolls from 0 to 1; where the answer
changes, the exact roll is found by halving the gap. While this runs, the
run's random state and the few fields those functions read are swapped for
stand-ins and put back afterwards. The lists are right for a run that has just
started, so they are taken then when possible and remembered per deck, stake
and challenge.

**Weighted games.** A mod can switch on Steamodded's "object weights"
(Pokermon does). That changes *how* the game picks, not just what exists,
and the library has a second way of predicting for it:

- Every item has a weight, and a roll picks one with a chance in proportion
  to it (`SMODS.poll_object`), instead of "item number N of a list".
  `selectByWeight` and `drawWeighted` in `filterprog.cpp` do the same
  arithmetic, step for step.
- Picks share random streams. The three blinds, the voucher, the two skip
  tags and the shop packs of an ante all draw from one stream, in the order
  the game makes them (`SeedFacts::needStart`): ante 1 is blinds, voucher,
  tags; later antes are voucher, tags, blinds; the packs follow. Every shop
  card, joker or consumable, draws from another. So a shop joker can only
  be predicted by predicting every card before it
  (`nextWeightedShopCard`).
- Weights change with what is on offer. An item in the shop can't come
  again, and a mod may change other weights too (Pokermon stops offering a
  Pokemon's relatives). The exporter finds these "links" by marking each
  item used in turn and weighing the lists again.

The exporter (`GD.build_weighted`) builds each list the way Steamodded
does, by calling the same Steamodded functions, then checks every list
against the game's real pick function with a few chosen rolls. If they ever
disagree (a newer Steamodded picks differently), the export fails with an
error instead of sending wrong lists, and the search is refused: with the
built-in lists nearly every seed would be predicted wrongly.

Two things about the game that the exporter has to work around: Balatro
switches Lua's garbage collector off and runs it by hand once per frame, so
a long export must collect its own garbage (`tidy()`), or a big mod pack
runs the game out of memory; and game code that "rolls again until it gets
an item" never ends if every stand-in roll is the same, so rolls come from a
sequence wherever such code is called.

**Limits.** Lists and numbers can be sent; code cannot. A mod that changes
how the game generates things in a way not described above is still
predicted wrongly. After every found run starts, the mod compares its ante 1
skip tags, voucher and boss with `bs_predict_start()` and tells the player
if they differ (`Brainstorm.checkPrediction`). In a weighted game the
predictions past the start of the run also assume that nothing else draws
from the shared streams first (a tag that adds a voucher, a boss reroll, a
mod's pack that makes cards without a stream of its own).

To compare the library with a real game, `bs_describe_seed()` prints
everything it predicts for a seed (`FilterProgram::describe`), in the same
form as a list of what the game really generated on that seed.

### Custom filters (`src/filterprog.hpp/.cpp`)
A custom filter is a list of **conditions**, written as text, which a seed
must all satisfy. The in-game filter builder writes this text; the mod passes
it to the library in `bs_params.filter_spec` (API version 2), where it is
read once per search, resolved against the game data above, and checked for
every seed. A condition that can never hold (a tag before the ante it
unlocks, an upgraded voucher on ante 1, the same legendary from two Souls
without `assume showman`) is an error with the reason, not an endless search.

```
tag     tag_charm      ante=1 blind=small     # Small Blind tag of ante 1
voucher v_telescope    ante=1
pack    p_arcana_mega  ante=1 within=2         # one of the first 2 shop packs
joker   j_blueprint    ante=1 within=4 edition=e_negative
soul    ante=1 packs=1 size=5 legendary=j_perkeo
boss    bl_final_heart ante=8                  # boss blind, never rerolled
```

Items are the game's own keys, so filters work in every language. The exact
meaning of each condition (e.g. vouchers assume earlier vouchers were bought,
bosses assume the boss is never rerolled)
is written down at the top of `filterprog.hpp`, as `referenceCheck()`: a
deliberately simple implementation that runs each condition on its own, with
the full generators.

`ante` may be a range (`ante=6-8`): the condition holds if it holds in any
of those antes. That is what replaces the old built-in Retcon ("by ante 8")
and Burglar ("antes 6-8") filters.

`matches()` is the fast version. It checks one kind of condition at a time,
cheapest first, and stops at the first failure. What a seed offers in an
ante is worked out only when a condition first needs it, and kept for the
others (`SeedFacts`): every `joker` condition of an ante reads the same shop
scan (`fastscan.hpp`), extended only as far as the longest one asks, and
"Burglar in antes 6-8" stops at ante 6 if it is there. That is safe because each kind
uses its own random streams. Vouchers run on a separate copy of the run,
because buying vouchers changes shop odds and editions, which the other
kinds assume untouched. The prefilter also uses a custom filter's ante 1
Small Blind tag, voucher and first random pack, so those filters get the
8-seeds-at-once fast rejection.

`tests/filterprog_test.cpp` builds thousands of random programs from
conditions that are true for some seed (so both outcomes happen) and checks
`matches()` against the AND of `referenceCheck()` over many seeds. It found a
real bug while being written: a few jokers changed rarity between game
versions (Vagabond: uncommon in 1.0.0, rare in 1.0.1), and the rarity lookup
must use the current version.

**Saved filters and the in-game builder** are `Brainstorm/UI/filter_builder.lua`.
The list of saved filters (Use, Edit, Delete, New filter) is the Filters
sub-tab of Settings > Brainstorm-Rerolled (`FB.list_rows()`, see below).
Filters are kept in
`Brainstorm.config.filters` (`{ active = n, list = { {name,
assume_enhanced, conditions}, ... } }`); the active one is turned into the
text above (`Brainstorm.FB.spec()`), which `searchOptions()` sends with
every search. The first start of this version imports older settings (the
first builder's single filter, the old built-in menu options) and adds four
starter filters (`FB.starter_filters()`): Charm + Perkeo, Charm + Negative
Perkeo, Charm + Perkeo + Observatory, and Buffoon + Neg. Blueprint (a
Negative Blueprint in the first Buffoon pack). Every old
built-in option can be rebuilt as conditions (`filterprog_test` checks
Charm + Soul, Perkeo, Observatory, Bean, Retcon... give the old answers;
Burglar is "Burglar in antes 6-8").

The filter editor screens (one filter, one condition, joker picker) live in
one window; Back from a filter returns to the Filters sub-tab
(`FB.to_settings()`). Inside the window moving between them replaces the window's contents in
place, the way the game's settings tabs do (`FB.go`), so nothing slides in.
Item lists and names come from the game itself (`G.P_CENTER_POOLS`,
`localize`). The joker list leaves out legendaries (use a Soul condition)
and Cavendish (appears only after Gros Michel dies; not searchable). Five
jokers only appear once the deck has a card with a certain enhancement
(Steel Joker, Stone Joker, Lucky Cat, Golden Ticket, Glass Joker); the
filter option "Assume 1 of each enhanced card" (`assume enhanced` in the
text) predicts the whole run as if the deck had one of each from the start,
which makes them searchable. Measured cost: in a run without those cards,
about 1 in 55 common/uncommon shop jokers differs (90% of the time it would
have been the gated joker itself); rare jokers, tags, vouchers, packs and
Souls never change.

Jokers (and the Soul's legendary) are chosen from a grid of real cards
built like the game's Collection screen (3 rows of 5, with pages);
`Card:click` is hooked only for cards marked `bs_pick`, every other card
behaves as before. "Save" asks the library to parse the whole filter first, so mistakes show a
message instead of a failing search.

The old named custom filters (Negative Blueprint, ...) were removed in
October 2026; `customFilters` keeps only `NO_FILTER`, so old saved settings
still load.

---

## 7. Floating-point exactness

Seed prediction only works if every `double` operation rounds **exactly** like
it does inside the game. Two compiler optimisations break that silently:

- **fast-math** (`-ffast-math`, `/fp:fast`) lets the compiler reorder and
  approximate arithmetic.
- **FMA contraction** fuses `a * b + c` into one instruction with one rounding
  instead of two. Clang enables it by default on Apple Silicon, so without
  care the Mac build would predict *different seeds* than the Windows build.

`CMakeLists.txt` therefore forces `-ffp-contract=off -fno-fast-math` (and
`/fp:precise` on MSVC). The old CMake file even had `-ffp-model=fast` for ARM,
which has been removed. CI runs the golden test on both Mac architectures to
prove they agree with the reference output.

> **Open question:** the reference output itself was generated on x86-64. We
> have not yet confirmed in-game on an Apple Silicon Mac that Balatro's own
> LuaJIT does not use FMA there. If it does, the Mac game would differ from
> Windows, and the Mac build would need contraction *on*. Checking this needs
> one Mac test: start a run on a seed the mod found and confirm the tag
> matches.

---

## 7b. Performance

Measure with the benchmark (`build/bench threads`). Every optimisation must
leave the golden output byte-for-byte identical, *and* be proven exact by its
own test.

**Where the time goes.** Almost entirely in producing random values. Each new
random stream ("Tag1", "Voucher1_resample2", ...) starts by hashing its name
on top of the seed: one 64-bit division per character, each waiting for the
previous one (~9 ns per character). Then a 13-digit rounding step and seeding
LuaJIT's generator (~28 ns).

| Optimisation | Where | Why it is exact | Gain |
|---|---|---|---|
| Fast paths in `fract()` / `round13()` | `util.cpp` | Used only where provably identical; original code otherwise. `tests/math_test.cpp` compares against verbatim copies of the originals over billions of inputs, incl. values on the exact rounding boundaries. | ~1.1x |
| Fixed-size stream cache instead of `std::map` | `instance.hpp` (`NodeCache`) | Same values stored, only the container changed; anything that doesn't fit goes to a `std::map` as before. | ~1.4x |
| Batched first check (prefilter) | `prefilter.cpp`, `scan.cpp` | Computes the first ante 1 tag/voucher/pack for 8 seeds at once so the CPU overlaps their division chains. It only *rejects* seeds whose roll it computed exactly (incl. re-rolls of locked items); all others go through the normal filter. `tests/prefilter_test.cpp` compares every roll with the normal engine. | ~1.6-2x for tag/voucher/pack filters |

| Skip streams that can't change the answer | `fastscan.cpp` | See below. `tests/fastscan_test.cpp` compares every shop item / pack with the full generators, and the filters with a copy of the original code. | Bean ~3.9x, Souls ~3x |

| LuaRandom by table lookup | `util.hpp/.cpp` (`luaRandomTop16`, `luaRandomDecide`) | See below. `tests/math_test.cpp` compares with the full generator over millions of seeds, incl. thresholds right at the bucket edges. | ~1.25x on shop scans, ~5-10% elsewhere |
| Branch-free stream step | `util.hpp` (`fractFast`, `round13Fast`, `floorSmall`) | Same arithmetic, written so the CPU never has to guess a coin-flip branch. Compared bit for bit with the originals by `math_test`. | ~1.1x; lets independent chains overlap 2x better |

Overall (15 threads, Charm Tag + voucher): 16.4 → 59 M seeds/s.
Single thread, Bean: 12k → 112k seeds/s; Souls: 0.75 → 2.6 M seeds/s.

**LuaRandom by table lookup.** `LuaRandom(seed)` makes four 64-bit states
from the seed, then runs 11 update steps before `random()` returns. Each step
only shifts, ANDs with constants and XORs. Such steps are *linear over bits*:
every output bit is the XOR of a fixed set of starting-state bits, whatever
the seed. So a table (built once, 16 KB) gives the contribution of each byte
of each state to the top 16 bits of `random()`, and 32 lookups XORed together
give those bits: 2.4x faster than running the steps.

16 bits only say that `random()` lies in an interval of width 1/65536. That
is enough for every question the engine asks of a fresh LuaRandom, because
they are all *monotone*: "is it below 0.714?", "which index does
`randint(0, 63)` give?", "which pack does the weighted choice give?".
`luaRandomDecide(seed, f)` evaluates `f` at both ends of the interval; if the
answers agree, every value in between gives the same answer. If not (about
once in 65536 draws), it runs the full generator. Used by `randint`,
`randchoice`, `randweightedchoice`, the prefilter and the fast scans.
`Instance::random()`, which returns the value itself, still runs it in full.

**Branch-free stream step.** Each stream step (`round13(fract(...))`) does
two roundings whose direction is a coin flip. Written with `if`/`?:`, the
compiler emits branches, the CPU guesses wrong half the time and throws away
everything it had started in parallel. `floorSmall` computes `floor` with
two adds (exact for 0 <= x < 2^52), and `oneIfGreater` turns the comparison
into a 0.0/1.0 mask with SSE2 (every x86-64 CPU has it; ARM compilers already
emit a branchless select).

**Tried and dropped:** computing several draws of a stream ahead (to overlap
them) gave nothing measurable: each draw is ~220 instructions, so the CPU's
reorder window only holds about one at a time anyway. Overlapping *different
seeds* (as the prefilter does) would help, but needs the shop scan
restructured per batch of seeds; not done yet.

**Skipping streams (Bean, Burglar, Souls).** Every random value comes from a
*named* stream, and each stream has its own state: drawing from `edisho8`
(edition of an ante 8 shop joker) never changes `cdt8` (card type of an ante 8
shop item). A full shop item draws from up to five streams:

```
nextShopItem(8):  cdt8          joker, tarot or planet?     <- needed
  nextJoker:      rarity8sho    common, uncommon or rare?   <- needed
                  edisho8       edition                     (skipped)
                  Joker2sho8    which uncommon joker        <- needed if uncommon
                  etperpoll8    stickers                    (skipped)
  (tarot/planet:  Tarotsho8 / Planetsho8                    (skipped))
```

Bean and Burglar only ask "is this item Turtle Bean / Burglar?", both
uncommon jokers, so `ShopJokerScan` draws only the streams marked "needed" and
leaves the rest untouched. In general a scan is told which rarities to
identify (any combination) and whether to work out editions; it then draws
the pools of those rarities, and the edition stream for *every* joker (so it
stays in step), and nothing else. The needed streams end up in exactly the state the
full generator leaves them in, so a later scan (Burglar's ante 8 after Bean's)
continues correctly. The one rule: nothing may read a *skipped* stream
afterwards on the same seed. Concretely: after scanning an ante's shop, don't
use the full generators for that ante, and scan it again only with the same
options (`ShopJokerScan::exact()` turns false if the options differ). The
current filters follow it (only Retcon follows, and it uses voucher streams).

**Building joker filters.** `findShopJoker(inst, ante, joker, items,
edition)` returns the shop position of the first matching joker (or -1), e.g.
`findShopJoker(inst, 1, Item::Blueprint, 4, Item::Negative)` = "a Negative
Blueprint among the first 4 ante 1 shop items". A filter that asks several
things about one ante's shop should use one `ShopJokerScan` with all the
rarities it needs (one rule above). These are the building blocks intended
for the in-game filter builder. Similarly, `arcanaPackHasSoul` only
draws the `soul_Tarot1` stream, not the five tarot choices; cases where the
tarot choices could matter (Omen Globe, Showman, a locked tarot) use the full
generator instead.

The stream lookups are also done once per scan (`Instance::node_ref`) instead
of once per draw, and the key strings are built once instead of per item.

## 8. Building for each platform

| Platform | Output | How |
|---|---|---|
| Windows (and Linux/Steam Deck via Proton) | `Immolate.dll` | MSVC, static runtime (no VC++ Redistributable needed) |
| macOS (Apple Silicon + Intel) | `Immolate.dylib` | Clang, one *universal* file containing both architectures |
| Linux (development only) | `Immolate.so` | GCC/Clang, used for local testing |
| Android phones (64-bit ARM) | `Immolate-android-arm64.so` | Android NDK (Clang), static C++ runtime |

**Windows builds are done locally** (free): on Linux, the MinGW-w64 cross
compiler builds `Immolate.dll` and Wine runs the tests (commands in the
README). **macOS builds need Apple's tools**, so they run on GitHub Actions:
`.github/workflows/build.yml` builds and tests on real Windows and macOS
machines, then publishes a ready-to-install `Brainstorm-Rerolled` folder as a
downloadable artifact. It only runs when started by hand (Actions tab ->
build -> Run workflow) to stay within the free minutes of a private repo.

**Android builds** need the free Android NDK (CI uses the one on GitHub's
Ubuntu machines). Locally, with the NDK unpacked in `~/Android`:

```
cmake -S Immolate -B Immolate/build-android \
  -DCMAKE_TOOLCHAIN_FILE=~/Android/android-ndk-r27c/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static
cmake --build Immolate/build-android --parallel
```

This also builds the test programs for Android. They can run on a phone
connected with USB debugging: `adb push` them to `/data/local/tmp` and run
them with `adb shell`. `filterprog_test` finds its data folder by a path
fixed when it is compiled, so for a phone it is compiled with
`-DTEST_DATA_DIR="/data/local/tmp/<folder>/data"` and `tests/data` is pushed
there.

To build locally: `Immolate/build.sh` (macOS/Linux) or `Immolate\build.bat`
(Windows, from the *x64 Native Tools Command Prompt for VS 2022*).

---

## 9. Tests

Run all of them with `ctest --test-dir build --output-on-failure`.

### Golden test (`tests/golden.cpp`)
Before anything was changed, this program was run against the **original**
code, and its output saved as `tests/golden_expected.txt`. It records:
- `[facts]`: tags, vouchers, packs, shop items, Mega Arcana contents and the
  Soul joker for 2,000 seeds spread across the seed space
- `[filters]`: for 17 filter settings, how many of 200,000 seeds pass, plus
  a hash of exactly *which* ones
- `[search]`: the seed a single-threaded search finds from a fixed start

Every change since then must reproduce that file byte-for-byte. **If you
change filter behaviour on purpose** (e.g. fixing a bug), regenerate it:
`build/golden > Immolate/tests/golden_expected.txt`, then review the diff
before committing. A good fix changes only the lines you expect. Example:
the lock fix (the original filters never set up the run-start locks) left `[facts]` untouched and only raised hit
counts, and a seed-by-seed comparison of old vs new filters showed no seed
was lost, only gained.

`tests/filter_adapter.hpp` is the only part that knows how filters are
implemented, so the harness survives refactors and optimisations.

### API test (`tests/api_test.cpp`)
Checks the C API's promises: known rare seeds are accepted, game keys and
English names select the same items, bad input gives errors instead of
crashes, cancelling is fast, wrap-around works, and the legacy function still
works.

### Exporter test (`tools/test-exporter.sh`, needs LuaJIT)
`Brainstorm/Core/gamedata.lua` runs outside Balatro against
`tools/mock_game.lua`, a stand-in for the unmodded game built from the
library's own data: what the exporter reads back through the mock's game
functions must equal the built-in data, for a fresh run and for one taken
mid-run. The text then goes through `Core/native.lua` into the real library
(a search, a seed check, the start prediction), and a third run adds modded
items to the mock. `ctest` runs it when LuaJIT is installed. This tests the
exporter's logic; only the real game can test it against the real game.

### In-game checks
The C++ tests compare the engine with itself and with the original code; the
final word is the real game. During development, predictions were compared
with what the game itself generates: filter verdicts per seed, the ante 1
skip tags, voucher and boss on unmodded and modded profiles, the boss blinds
of antes 1..8 (kept in `tests/boss_test.cpp`), and whole runs in a weighted
game (kept in `tests/data`).

Never build real `Card` objects in such checks: deleting them at once leaves
their buttons behind and crashes the next run. Call the same functions as
`create_card` and stop before the card is built.

---

## 10. File map

```
Brainstorm/                    the mod (players install it as Brainstorm-Rerolled/)
  lovely.toml                  tells Lovely what to inject
  Core/Brainstorm.lua          keybinds, save states, auto-reroll flow
  Core/native.lua              FFI bridge to the native library
  UI/ui.lua                    adds the Brainstorm-Rerolled settings tab
  UI/settings.lua              settings tab: Filters / Saved seeds / Hotkeys / Mobile
  UI/mobile.lua                touch buttons (the Mobile Friendly setting)
  UI/filter_builder.lua        in-game custom filter builder
  nativefs.lua                 file access outside the game's sandbox (3rd party)
Immolate/
  CMakeLists.txt               build definition (all platforms)
  src/brainstorm_api.h/.cpp    C API + threaded search engine     <- new
  src/filters.hpp/.cpp         Brainstorm's filters               <- moved from immolate.cpp
  src/prefilter.hpp/.cpp       batched first check (performance)
  src/scan.hpp/.cpp            "first matching seed in a range" (used by workers)
  src/fastscan.hpp/.cpp        fast shop joker scans (findShopJoker) and Soul check
  src/filterprog.hpp/.cpp      custom filters: text format, reference meaning, fast check
  src/immolate.hpp/.cpp        legacy blocking brainstorm() export
  src/instance.hpp             per-seed RNG state, randchoice
  src/functions.hpp            nextTag/nextPack/nextJoker/... (game logic)
  src/items.hpp/.cpp           every item, pool, and name table
  src/seed.hpp/.cpp            seed <-> ID, cached hashing
  src/util.hpp/.cpp            pseudohash, LuaRandom, round13
  src/main.cpp, search.hpp     original dev CLI (immol), not used by the mod
  tests/                       golden, API, math, prefilter, fastscan, filterprog tests + bench
tools/                         test-exporter.sh, mock_game.lua (the exporter test)
docs/
  ARCHITECTURE.md              this file
.github/workflows/build.yml    CI: build + test + package (+ release)
thunderstore/                  manifest.json, README.md, icon.png for the Thunderstore package
```
