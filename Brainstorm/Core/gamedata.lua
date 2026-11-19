--[[
  gamedata.lua — asks the running game for its item lists and odds, so the
  native searcher predicts THIS game (mods, deck, challenge) instead of the
  unmodded lists built into the library.

  The result is text, in the format described in Immolate/src/gamedata.hpp:

      local text, err = Brainstorm.gamedata.get()   -- nil, reason on failure

  HOW
    Nothing here knows what a mod added. Every list comes from the game's own
    functions, called the way the game calls them:
      - get_current_pool() for tags, vouchers and jokers;
      - get_pack(), poll_edition() and get_current_pool()'s rarity roll are
        "probed": the random roll is replaced by a number we choose, and the
        answer is recorded for rolls from 0 to 1. Where the answer changes,
        the exact roll is found by halving the gap.
    While this runs, the game's random state and the few fields these
    functions read are swapped for stand-ins, then put back (sandbox()), so
    the current run is never disturbed.

  WHEN
    Lists are right for a run that has just started (nothing bought or
    used). get() takes them at that moment when it can and remembers them
    per deck, stake and challenge; started mid-run, it resets what it can
    (bought vouchers, owned jokers) and the result may be slightly off.

  LIMITS
    Lists and numbers are sent; code is not. A mod that changes how the game
    generates things is predicted wrongly; Brainstorm.lua compares every
    found run with the prediction and tells the player.
]]

local GD = {}

-- One word of the native text formats: '%', whitespace and control
-- characters as %XX, so a key never splits in two. Mods may use spaces in
-- keys (Cryptid's "j_cry_Double Scale"); the library decodes them again
-- (GameData::decodeWord in Immolate/src/gamedata.cpp).
function GD.encode(word)
  word = tostring(word):gsub("[%%%s%c]", function(c)
    return string.format("%%%02X", c:byte())
  end)
  -- A word starting with '*' is a weight in a list (weighted games).
  return (word:gsub("^%*", "%%2A"))
end

-- Progress notes for development (set GD.trace to a function to see them).
local function trace(what)
  if GD.trace then
    GD.trace(what)
  end
end

local MAX_ANTE = 8
-- Largest roll below 1 (rolls are in [0, 1)).
local LAST_ROLL = 1 - 2 ^ -53

-- The roll every stubbed pseudorandom() call returns while probing.
local roll = 0.5
-- Or, when set, a different roll per call: { base = x, n = 0 } gives x,
-- x + g, x + 2g, ... (fractional parts; g is the golden ratio, which spreads
-- them evenly). For game code that rolls again until it gets an item.
local sequence = nil
-- Stubbed rolls handed out since the last fresh_rolls(). Game code that
-- "rolls again until it gets an item" never ends when every roll is the
-- same and lands on an empty slot; this stops it with an error instead of
-- freezing the game.
local rolls_made = 0
-- Balatro switches Lua's garbage collector off and runs it by hand once per
-- frame (nuGC in functions/misc_functions.lua). Everything here happens
-- inside one frame, so nothing the game's functions allocate is freed until
-- it ends: asking a big modded game for its joker list a few hundred times
-- used up all memory and took the game down. tidy() collects by hand
-- whenever some tens of megabytes have piled up.
local tidy_mark = 0
local function tidy()
  if collectgarbage("count") - tidy_mark > 48 * 1024 then
    collectgarbage("collect")
    tidy_mark = collectgarbage("count")
  end
end
local function fresh_rolls()
  rolls_made = 0
  tidy()
end
local function next_roll()
  rolls_made = rolls_made + 1
  if rolls_made > 200000 then
    error("the game keeps rolling without picking anything", 0)
  end
  if not sequence then
    return roll
  end
  local v = (sequence.base + sequence.n * 0.6180339887498949) % 1
  sequence.n = sequence.n + 1
  return v
end
-- Counts pseudoseed() calls for one key (see rarity_polls()).
local count_key, count = nil, 0
-- When set, collects the stream names pseudoseed() is asked for.
local seen_streams = nil

local function copy(list)
  local out = {}
  for i = 1, #list do
    out[i] = list[i]
  end
  return out
end

local function set_of(list)
  local out = {}
  for i = 1, #list do
    out[list[i]] = true
  end
  return out
end

local function num(v)
  return type(v) == "number" and v or 0
end

-- %.17g prints a double so that reading it back gives the same double.
local function number(v)
  return string.format("%.17g", v)
end

-- Records fn(roll) for rolls 0..1: { { from = roll, value = v }, ... }, one
-- entry per stretch of equal answers. `grid` evenly spaced rolls are tried;
-- between two that differ, the changes are found by halving.
local function steps(fn_unguarded, grid)
  local out = {}
  local function fn(x)
    fresh_rolls()
    return fn_unguarded(x)
  end
  local function first_change(lo, hi, v_lo) -- fn(lo) == v_lo, fn(hi) ~= v_lo
    for _ = 1, 64 do
      local mid = lo + (hi - lo) / 2
      if mid <= lo or mid >= hi then
        break
      end
      if fn(mid) == v_lo then
        lo = mid
      else
        hi = mid
      end
    end
    return hi
  end
  local last_x, last_v
  for i = 0, grid do
    local x = math.min(i / grid, LAST_ROLL)
    local v = fn(x)
    if i == 0 then
      out[1] = { from = 0, value = v }
    elseif v ~= last_v then
      local lo, v_lo = last_x, last_v
      for _ = 1, 64 do -- usually one change; several if the grid is coarse
        local at = first_change(lo, x, v_lo)
        local v_at = fn(at)
        out[#out + 1] = { from = at, value = v_at }
        lo, v_lo = at, v_at
        if v_at == v then
          break
        end
      end
    end
    last_x, last_v = x, v
  end
  return out
end

-- Whether the run is exactly as it started: nothing played, skipped, bought.
function GD.is_fresh()
  local gm = G.GAME
  if not (G.STAGE == G.STAGES.RUN and gm and gm.round_resets) then
    return false
  end
  local states = gm.round_resets.blind_states
  return gm.round_resets.ante == 1
    and (gm.round or 0) == 0
    and G.STATE == G.STATES.BLIND_SELECT
    and (not states or states.Small == "Select")
end

-- Deck, stake and challenge: what the lists depend on besides the mods.
function GD.cache_key()
  local gm = G.GAME or {}
  local back = gm.selected_back
  return table.concat({
    tostring(back and (back.name or (back.effect and back.effect.center and back.effect.center.key)) or "?"),
    tostring(gm.stake or "?"),
    tostring(gm.challenge or ""),
  }, "|")
end

-- Runs fn with the game set up for asking about a fresh run, and restores
-- everything afterwards, error or not. Returns ok, result-or-error.
local function sandbox(fresh, fn)
  local gm = G.GAME
  local saved = {
    pr = gm.pseudorandom,
    ante = gm.round_resets.ante,
    choices = gm.round_resets.blind_choices,
    fsb = gm.first_shop_buffoon,
    uv = gm.used_vouchers,
    uj = gm.used_jokers,
    pf = gm.pool_flags,
    sv = G.shop_vouchers,
    pc = G.playing_cards,
    bu = gm.bosses_used,
    tags = gm.tags,
    paused = G.SETTINGS.paused,
    pseudorandom = pseudorandom,
    pseudoseed = pseudoseed,
    rates = {},
  }
  tidy_mark = collectgarbage("count")

  -- A watchdog for development (only while a tool has set GD.trace): Lua
  -- can call a function every so many instructions, but only for code it
  -- interprets, so its compiler is off meanwhile, which makes everything
  -- several times slower. Past the time limit the function notes where the
  -- code is and raises an error.
  local clock = love and love.timer and love.timer.getTime or os.clock
  local deadline = clock() + GD.MAX_SECONDS
  local watched = false
  local old_hook, old_mask, old_count
  if GD.trace and jit and jit.off and debug and debug.sethook then
    old_hook, old_mask, old_count = debug.gethook()
    jit.off()
    jit.flush()
    debug.sethook(function()
      if clock() > deadline then
        deadline = deadline + 0.5 -- (again soon, if something swallows the error)
        trace("WATCHDOG" .. debug.traceback("", 2))
        error("reading the game's item lists takes too long" .. debug.traceback("", 2), 0)
      end
    end, "", 100000)
    watched = true
  end

  -- The roll is ours; the run's random streams are not touched at all.
  -- pseudoseed() gives a different number each time, as in the game: code
  -- that seeds its own picks with it must not get the same pick forever.
  local seeds_made = 0
  pseudoseed = function(key)
    if key == count_key then
      count = count + 1
    end
    if seen_streams then
      seen_streams[#seen_streams + 1] = tostring(key)
    end
    seeds_made = seeds_made + 1
    return 0.05 + 0.9 * ((seeds_made * 0.6180339887498949) % 1)
  end
  pseudorandom = function(seed, min, max)
    if type(seed) == "string" then
      pseudoseed(seed)
    end
    local r = next_roll()
    if min and max then
      return math.min(max, min + math.floor(r * (max - min + 1)))
    end
    return r
  end
  gm.pseudorandom = { seed = "BSLISTS1", hashed_seed = 0.5 }
  gm.round_resets.blind_choices = {}
  -- No boss seen yet (the run's own boss is already counted).
  if SMODS and SMODS.normalize_bosses_used_table then
    gm.bosses_used = SMODS.normalize_bosses_used_table({})
  end
  trace("sandbox ready")
  gm.first_shop_buffoon = true -- get_pack: past the fixed first pack
  G.shop_vouchers = nil -- vouchers on sale are left out of the list
  G.playing_cards = G.playing_cards or {}
  G.SETTINGS.paused = false -- (paused, the game hands out throwaway rolls)
  if not fresh then
    -- Mid-run: undo what the run changed, as far as that can be known.
    local start_vouchers = {}
    local config = gm.selected_back and gm.selected_back.effect and gm.selected_back.effect.config or {}
    if config.voucher then
      start_vouchers[config.voucher] = true
    end
    for _, v in ipairs(config.vouchers or {}) do
      start_vouchers[v] = true
    end
    gm.used_vouchers = start_vouchers
    gm.used_jokers = {}
    gm.pool_flags = {}
    local ok, base = pcall(function()
      return G:init_game_object()
    end)
    if ok and type(base) == "table" then
      for k, v in pairs(base) do
        if type(k) == "string" and k:match("_rate$") and type(v) == "number" then
          saved.rates[k] = gm[k]
          gm[k] = config[k] or v
        end
      end
    end
  end

  local ok, result = xpcall(fn, debug.traceback)

  if watched then
    debug.sethook(old_hook, old_mask, old_count)
    jit.on()
  end
  pseudorandom, pseudoseed = saved.pseudorandom, saved.pseudoseed
  gm.pseudorandom = saved.pr
  gm.round_resets.ante = saved.ante
  gm.round_resets.blind_choices = saved.choices
  gm.bosses_used, gm.tags = saved.bu, saved.tags
  sequence, seen_streams = nil, nil
  gm.first_shop_buffoon = saved.fsb
  gm.used_vouchers, gm.used_jokers, gm.pool_flags = saved.uv, saved.uj, saved.pf
  G.shop_vouchers, G.playing_cards = saved.sv, saved.pc
  G.SETTINGS.paused = saved.paused
  for k, v in pairs(saved.rates) do
    gm[k] = v
  end
  if G.ARGS and G.ARGS.TEMP_POOL then
    G.ARGS.TEMP_POOL = EMPTY and EMPTY(G.ARGS.TEMP_POOL) or {}
  end
  count_key = nil
  return ok, result
end

-- A list from get_current_pool as words: keys, "key!" for a slot that is
-- unavailable (the game's 'UNAVAILABLE' placeholder), "key?" for a joker
-- that only needs an enhanced card in the deck, "-" if the key is unknown.
-- `raw` is the game's full list in the same order, when one lines up.
local function words(pool, raw)
  local aligned = raw and #raw == #pool
  if aligned then
    for i = 1, #pool do
      if pool[i] ~= "UNAVAILABLE" and pool[i] ~= raw[i].key then
        aligned = false
        break
      end
    end
  end
  local banned = G.GAME.banned_keys or {}
  local out = {}
  for i = 1, #pool do
    local key = pool[i]
    if key ~= "UNAVAILABLE" then
      out[i] = key
    elseif not aligned then
      out[i] = "-"
    else
      local c = raw[i]
      local gated = c.enhancement_gate and c.unlocked ~= false and not banned[c.key]
      out[i] = c.key .. (gated and "?" or "!")
    end
  end
  return out -- a table; add() below encodes each entry
end

-- The game's full list a pool was built from: the one of the same length
-- whose keys match every available slot.
local function raw_list_for(pool, candidates)
  for _, raw in pairs(candidates or {}) do
    if type(raw) == "table" and #raw == #pool then
      local same = true
      for i = 1, #pool do
        if pool[i] ~= "UNAVAILABLE" and (type(raw[i]) ~= "table" or raw[i].key ~= pool[i]) then
          same = false
          break
        end
      end
      if same then
        return raw
      end
    end
  end
  return nil
end

-- Whatever poll_edition returned, as an edition key ("-" for none).
local function edition_key(e)
  if type(e) == "string" then
    return e
  elseif type(e) == "table" then
    if e.key then
      return e.key
    end
    for k, v in pairs(e) do
      if v == true and G.P_CENTERS["e_" .. k] then
        return "e_" .. k
      end
    end
  end
  return "-"
end

-- Does the game keep a used item's slot as 'UNAVAILABLE' (stock), or drop
-- it from the list (some mods)? Mark one tarot used and look.
local function keeps_slots()
  local gm = G.GAME
  local before = copy(get_current_pool("Tarot"))
  local probe
  for _, key in ipairs(before) do
    if key ~= "UNAVAILABLE" and not gm.used_jokers[key] then
      probe = key
      break
    end
  end
  if not probe then
    return true
  end
  gm.used_jokers[probe] = true
  local ok, after = pcall(function()
    return copy(get_current_pool("Tarot"))
  end)
  gm.used_jokers[probe] = nil
  return not ok or #after == #before
end

-- Builds the text. Runs inside sandbox().
-- Mods' Soul-like consumables (Steamodded's SMODS.Consumable.legendaries):
-- a pack card of a matching set (their type, or their soul_set) rolls for
-- them before The Soul. One "modsoul" line each, through add().
local function add_modsouls(add)
  local legendaries = SMODS and SMODS.Consumable and SMODS.Consumable.legendaries
  for _, v in ipairs(type(legendaries) == "table" and legendaries or {}) do
    local ok, in_pool = pcall(SMODS.add_to_pool, v)
    if not ok or in_pool then
      local sets, seen = {}, {}
      for _, s in ipairs({ v.type and v.type.key, v.soul_set }) do
        if (s == "Tarot" or s == "Planet" or s == "Spectral") and not seen[s] then
          seen[s] = true
          sets[#sets + 1] = s
        end
      end
      if #sets > 0 then
        local unpack_all = table.unpack or unpack -- (all of them: not inside "and"/"or")
        add("modsoul", v.key, number(num(v.soul_rate)), v.can_repeat_soul and "1" or "0", unpack_all(sets))
      end
    end
  end
end

local function build(fresh, fresh_first_pack)
  local gm = G.GAME
  local banned = gm.banned_keys or {}
  local lines = { "gamedata 1" }
  -- Each argument is one word, or a table of words (a list); every word is
  -- encoded here. (The marks ? and ! and "-" are left as they are.)
  local function add(...)
    local parts = {}
    for i = 1, select("#", ...) do
      local v = select(i, ...)
      if type(v) == "table" then
        for _, k in ipairs(v) do
          parts[#parts + 1] = GD.encode(k)
        end
      else
        parts[#parts + 1] = GD.encode(v)
      end
    end
    lines[#lines + 1] = table.concat(parts, " ")
  end
  lines[#lines + 1] = table.concat({ "# deck/stake/challenge:", GD.cache_key(), fresh and "(fresh run)" or "(taken mid-run)" }, " ")

  if SMODS and SMODS.optional_features and SMODS.optional_features.object_weights then
    return GD.build_weighted(fresh, fresh_first_pack, lines, add)
  end

  add("pad", keeps_slots() and "1" or "0")

  -- Tags: the list changes with the ante (some need ante 2).
  for a = 1, MAX_ANTE do
    gm.round_resets.ante = a
    add("tags", tostring(a), words(copy(get_current_pool("Tag")), G.P_CENTER_POOLS.Tag))
  end
  gm.round_resets.ante = 1

  -- Vouchers: available now, and once the vouchers they require are bought.
  local start = set_of(copy(get_current_pool("Voucher")))
  local used = gm.used_vouchers
  for _, v in ipairs(G.P_CENTER_POOLS.Voucher) do
    local later, needs = start[v.key], {}
    if type(v.requires) == "table" and next(v.requires) then
      local with = {}
      for k, val in pairs(used) do
        with[k] = val
      end
      for _, r in pairs(v.requires) do
        with[r] = true
        needs[#needs + 1] = r
      end
      gm.used_vouchers = with
      later = set_of(copy(get_current_pool("Voucher")))[v.key]
      gm.used_vouchers = used
    end
    add("voucher", v.key, start[v.key] and "1" or "0", later and "1" or "0", needs)
  end

  -- Packs. "p_arcana_normal_1" .. "_4" only differ in artwork.
  if fresh_first_pack and not banned["p_buffoon_normal_1"] then
    add("firstpack", "p_buffoon_normal")
  end
  local pack_steps = steps(function(x)
    roll = x
    local c = get_pack("shop_pack")
    local key = c and c.key or "?"
    return key:match("^(.*)_%d+$") or key
  end, 4096)
  for _, s in ipairs(pack_steps) do
    add("pack", number(s.from), s.value)
  end

  -- Shop: the chance a card is a joker (create_card_for_shop: the joker
  -- rate comes first), and how many cards one shop shows.
  local total = num(gm.joker_rate) + num(gm.playing_card_rate)
  local types = SMODS and SMODS.ConsumableType and SMODS.ConsumableType.obj_buffer
  if type(types) == "table" then
    for _, v in ipairs(types) do
      total = total + num(gm[tostring(v):lower() .. "_rate"])
    end
  else
    total = total + num(gm.tarot_rate) + num(gm.planet_rate) + num(gm.spectral_rate)
  end
  add("shop", number(total), number(num(gm.joker_rate)), tostring(gm.shop and gm.shop.joker_max or 2))
  -- The other kinds of shop card, in create_card_for_shop's order after
  -- the jokers (Tarot, Planet, playing card, Spectral; a mod's own kinds
  -- come after those and aren't predicted).
  add("shoprates", number(num(gm.tarot_rate)), number(num(gm.planet_rate)),
    number(num(gm.playing_card_rate)), number(num(gm.spectral_rate)))
  -- Tarot, Planet and Spectral cards as get_current_pool gives them (the
  -- pool is the same for packs and the shop; only its stream name differs).
  for _, set in ipairs({ "Tarot", "Planet", "Spectral" }) do
    if G.P_CENTER_POOLS[set] then
      local pool = copy(get_current_pool(set, nil, nil, "sho"))
      add("cards", set, words(pool, raw_list_for(pool, { G.P_CENTER_POOLS[set] })))
    end
  end
  add_modsouls(add)

  -- Joker rarities: how often get_current_pool rolls for the rarity, which
  -- list each roll gives, and the lists.
  count_key, count = "rarity1sho", 0
  roll = 0.5
  get_current_pool("Joker", nil, nil, "sho")
  add("raritypolls", tostring(math.max(1, math.min(4, count))))
  count_key = nil
  local pools = {}
  local rarity_steps = steps(function(x)
    roll = x
    local pool, key = get_current_pool("Joker", nil, nil, "sho")
    key = tostring(key)
    if not pools[key] then
      pools[key] = copy(pool)
    end
    return key
  end, 512)
  for _, s in ipairs(rarity_steps) do
    local pool = pools[s.value]
    -- The stream name ends with the ante ("Joker1sho1"); it is sent without.
    local stream = s.value:sub(-1) == "1" and s.value:sub(1, -2) or s.value
    add("rarity", number(s.from), stream, words(pool, raw_list_for(pool, G.P_JOKER_RARITY_POOLS)))
  end

  -- Editions of shop jokers.
  local edition_steps = steps(function(x)
    roll = x
    return edition_key(poll_edition("edisho1"))
  end, 4096)
  for i, s in ipairs(edition_steps) do
    if i > 1 or s.value ~= "-" then
      if s.from == 0 then
        error("every shop joker gets an edition here, which the searcher can't express", 0)
      end
      add("edition", number(s.from), s.value)
    end
  end

  -- The Soul, and the legendaries it gives.
  add("soul", (banned["c_soul"] or gm.used_jokers["c_soul"]) and "0" or "1")
  -- Spectral cards roll for a Black Hole too (create_card), unless it is
  -- banned or in use; a banned Soul switches both rolls off.
  add("blackhole", (banned["c_soul"] or banned["c_black_hole"] or gm.used_jokers["c_black_hole"]) and "0" or "1")
  local legendary, legendary_key = get_current_pool("Joker", nil, true, "sou")
  legendary = copy(legendary)
  add("legendary", tostring(legendary_key), words(legendary, raw_list_for(legendary, G.P_JOKER_RARITY_POOLS)))

  -- Bosses, sorted the way the game sorts them, with the antes they can be
  -- the boss of.
  local bosses = {}
  for key, blind in pairs(G.P_BLINDS) do
    if blind.boss then
      bosses[#bosses + 1] = key
    end
  end
  table.sort(bosses)
  local antes = {}
  for _, key in ipairs(bosses) do
    antes[key] = ""
  end
  for a = 1, MAX_ANTE do
    gm.round_resets.ante = a
    local eligible = {}
    if SMODS and SMODS.create_blind_pool then
      eligible = set_of(SMODS.create_blind_pool("boss", true))
    else
      -- get_new_boss() in the game's functions/common_events.lua.
      local win = gm.win_ante or 8
      for _, key in ipairs(bosses) do
        local boss = G.P_BLINDS[key].boss
        if boss.showdown then
          eligible[key] = a % win == 0 and a >= 2
        else
          eligible[key] = boss.min <= math.max(1, a) and (math.max(1, a) % win ~= 0 or a < 2)
        end
        eligible[key] = eligible[key] and not banned[key]
      end
    end
    for _, key in ipairs(bosses) do
      if eligible[key] then
        antes[key] = antes[key] .. a
      end
    end
  end
  gm.round_resets.ante = 1
  for _, key in ipairs(bosses) do
    add("boss", key, antes[key] ~= "" and antes[key] or "-")
  end

  return table.concat(lines, "\n") .. "\n"
end

-------------------------------------------------------------------------------
-- Weighted games (Steamodded's "object weights", switched on by a mod)
--
-- The game then picks every item by weight (SMODS.poll_object in
-- Steamodded's src/utils/weights.lua) instead of "item number N of a list".
-- What is sent: each list with its weights, the names of the streams the
-- picks share, and what changes while an item is on offer. The format is in
-- Immolate/src/gamedata.hpp ("WEIGHTED GAMES").
--
-- The lists are built the way poll_object builds them, by calling the same
-- Steamodded functions. Because that repeats some of Steamodded's own code,
-- every list is then CHECKED against the game's real function with a few
-- chosen rolls; if they ever disagree (a new Steamodded picks differently),
-- the export stops with an error instead of sending wrong lists.
-------------------------------------------------------------------------------

local UNAVAILABLE = "UNAVAILABLE"

-- The items SMODS.poll_object(args) would pick from, before weights:
-- the first lines of poll_object.
local function members_of(args)
  fresh_rolls()
  args.types = { args.type }
  local pool, types_used = SMODS.create_poll_pool(args.types, args)
  if args.filter then
    pool = args.filter(pool)
  end
  return { pool = pool, types_used = types_used, args = args }
end

-- Their weights right now: { { key, weight }, ... }. The loop in the middle
-- of poll_object.
local function weigh(members)
  local pool, types_used, args = members.pool, members.types_used, members.args
  tidy()
  local weights = {}
  for _, entry in ipairs(pool) do
    local key = entry.key or entry
    local where = SMODS.game_table_from_type[entry.type]
    local objects = G.P_CENTERS
    if where then
      objects = type(where) == "table" and where.ref_table[where.ref_value] or G[where]
    end
    local _, weight = SMODS.get_weight_of_object(objects[key], entry.weight, args)
    local w = { key = key, weight = weight }
    weights[#weights + 1] = w
    weights[key] = w -- (two entries with one key share the last one's weight)
  end
  -- Mods and jokers may change weights here (Pokermon does).
  SMODS.calculate_context({ modify_weights = true, pool = weights, pool_types = types_used })
  local out = {}
  for i, w in ipairs(weights) do
    local weight = weights[w.key].weight
    if type(weight) ~= "number" or weight < 0 or weight ~= weight then
      error("item " .. tostring(w.key) .. " has no usable weight", 0)
    end
    out[i] = { key = w.key, weight = weight }
  end
  return out
end

-- The list SMODS.poll_object(args) would pick from: { { key, weight }, ... }.
local function weighted_list(args)
  return weigh(members_of(args))
end

-- Which entry a roll picks: the second half of poll_object, with
-- SMODS.select_by_weight written as a loop. (The library does the same
-- arithmetic: selectByWeight in Immolate/src/filterprog.cpp.)
local function select_weighted(list, x)
  local running, total = {}, 0
  for i, e in ipairs(list) do
    total = total + e.weight
    running[i] = total
  end
  local poll = total - x * total
  if not (poll > running[1]) then
    return 1
  end
  local low, high = 1, #list
  while high - low > 1 do
    local check = math.floor((low + high) / 2)
    if poll < running[check] then
      high = check
    else
      low = check
    end
  end
  return high
end

-- One pick as the game makes it, taking rolls from next(): a throwaway roll
-- (poll_object's "does anything come up at all" test, always passed by the
-- kinds of item handled here), then the roll that picks; again while the
-- slot has no item.
local function pick_weighted(list, next)
  for _ = 1, 1000 do
    next()
    local e = list[select_weighted(list, next())]
    if e.key ~= UNAVAILABLE then
      return e.key
    end
  end
  return nil
end

-- Compares the game's real pick (real()) with ours (mine(next)) for a few
-- sequences of rolls. Any difference stops the export.
local function check(what, real, mine)
  trace("check " .. what)
  for i = 1, 16 do
    local base = (0.0131 + i * 0.0797) % 1
    sequence = { base = base, n = 0 }
    fresh_rolls()
    local got = real()
    sequence = { base = base, n = 0 }
    fresh_rolls()
    local want = mine(next_roll)
    sequence = nil
    if got ~= want then
      error(("%s: the game picks %s where the searcher expects %s; this version of Steamodded isn't supported yet"):format(what, tostring(got), tostring(want)), 0)
    end
  end
end

-- The stream a real pick takes its deciding roll from (the last one asked
-- for), without the ante at its end.
local function stream_of(real)
  trace("stream_of")
  sequence = { base = 0.37, n = 0 }
  seen_streams = {}
  fresh_rolls()
  real()
  local name = seen_streams[#seen_streams]
  sequence, seen_streams = nil, nil
  local ante = tostring(G.GAME.round_resets.ante)
  if type(name) ~= "string" or name:sub(-#ante) ~= ante then
    error("can't tell which random stream the game uses (" .. tostring(name) .. ")", 0)
  end
  return name:sub(1, -#ante - 1)
end

-- The key_append the game passes to create_card for a shop card ("sho" in
-- the stock game; nothing in Steamodded's weighted shop). It decides the
-- name of the edition stream. Found by letting the game start to make a
-- shop card with create_card replaced by a recorder that stops it there:
-- no card is ever built.
local function shop_card_append(default)
  local STOP = {}
  local real_create, got = create_card, nil
  local gm = G.GAME
  local tags = gm.tags
  gm.tags = {} -- (a tag could put its own card in the shop)
  create_card = function(...)
    got = { n = select("#", ...), ... }
    error(STOP, 0)
  end
  sequence = { base = 0, n = 0 } -- the first roll, 0, makes the card a joker
  fresh_rolls()
  local area = { T = { x = 0, y = 0, w = 0, h = 0 }, config = {}, cards = {} }
  pcall(create_card_for_shop, area)
  sequence = nil
  create_card = real_create
  gm.tags = tags
  -- What SMODS.create_card had set for the card that was never made.
  for _, k in ipairs({ "bypass_create_card_edition", "bypass_create_card_discover", "bypass_create_card_discovery_center",
    "set_create_card_front", "create_card_allow_duplicates", "create_card_scale", "create_card_silent_edition" }) do
    SMODS[k] = nil
  end
  if not got then
    return default
  end
  return got[8] or ""
end

-- A weighted list as words: "*<weight>" before the entries it applies to,
-- "-" for a slot without an item, "key?" for an item of `gated` (only there
-- with an enhanced card in the deck).
local function weighted_words(list, gated, rename)
  local out, weight = {}, 10
  for _, e in ipairs(list) do
    if e.weight ~= weight then
      weight = e.weight
      out[#out + 1] = { raw = "*" .. number(weight) }
    end
    if e.key == UNAVAILABLE then
      out[#out + 1] = { raw = "-" }
    else
      local key = rename and rename(e.key) or e.key
      out[#out + 1] = { raw = GD.encode(key) .. ((gated and gated[e.key]) and "?" or "") }
    end
  end
  return out
end

function GD.build_weighted(fresh, fresh_first_pack, lines, add_words)
  local gm = G.GAME
  local banned = gm.banned_keys or {}
  if G.STAGE ~= G.STAGES.RUN then
    error("start a run first (this modded game's item lists can only be read during a run)", 0)
  end
  -- add(): like build()'s, but a { raw = "..." } word is written as it is.
  local function add(...)
    local parts = {}
    local function put(v)
      if type(v) == "table" and v.raw then
        parts[#parts + 1] = v.raw
      elseif type(v) == "table" then
        for _, k in ipairs(v) do
          put(k)
        end
      else
        parts[#parts + 1] = GD.encode(v)
      end
    end
    for i = 1, select("#", ...) do
      put((select(i, ...)))
    end
    lines[#lines + 1] = table.concat(parts, " ")
  end
  local function pack_kind(key)
    return key:match("^(.*)_%d+$") or key -- "_1".."_4" only differ in artwork
  end

  add("weights", "1")
  trace("weighted: start")

  ---------------------------------------------------------------------------
  -- Tags, vouchers, blinds, packs: one shared stream per ante.
  local function tag_list()
    return weighted_list({ type = "Tag" })
  end
  local type_stream = stream_of(get_next_tag_key)
  local tags1 = tag_list()
  check("skip tags", get_next_tag_key, function(next)
    return pick_weighted(tags1, next)
  end)
  trace("tags per ante")
  for a = 1, MAX_ANTE do
    gm.round_resets.ante = a
    add("tags", tostring(a), weighted_words(tag_list()))
  end
  trace("vouchers")
  gm.round_resets.ante = 1

  -- Vouchers: weight, available now, and once the vouchers they require are
  -- bought (as in build()).
  local function voucher_list()
    local list = weighted_list({ type = "Voucher" })
    local by_key = {}
    for _, e in ipairs(list) do
      if e.key == UNAVAILABLE then
        error("the voucher list has empty slots, which the searcher can't express", 0)
      end
      by_key[e.key] = e.weight
    end
    return list, by_key
  end
  local vouchers_now, start = voucher_list()
  check("vouchers", function()
    return (SMODS.get_next_vouchers() or {})[1]
  end, function(next)
    return pick_weighted(vouchers_now, next)
  end)
  trace("voucher requirements")
  local used = gm.used_vouchers
  for _, v in ipairs(G.P_CENTER_POOLS.Voucher) do
    local later, needs = start[v.key], {}
    if type(v.requires) == "table" and next(v.requires) then
      local with = {}
      for k, val in pairs(used) do
        with[k] = val
      end
      for _, r in pairs(v.requires) do
        with[r] = true
        needs[#needs + 1] = r
      end
      gm.used_vouchers = with
      local _, by_key = voucher_list()
      later = by_key[v.key]
      gm.used_vouchers = used
    end
    add("voucher", v.key, start[v.key] and "1" or "0", later and "1" or "0",
      { raw = "*" .. number(start[v.key] or later or 10) }, needs)
  end
  add("vouchersperante", tostring(math.max(1, math.min(8,
    num(gm.starting_params and gm.starting_params.vouchers_in_shop) + num(gm.modifiers and gm.modifiers.extra_vouchers)))))

  -- Bosses: the antes each can be the boss of, and its weight.
  trace("bosses")
  local bosses = {}
  for key, blind in pairs(G.P_BLINDS) do
    if blind.boss then
      bosses[#bosses + 1] = key
    end
  end
  table.sort(bosses)
  local antes, boss_weight = {}, {}
  for _, key in ipairs(bosses) do
    antes[key] = ""
  end
  local function boss_list()
    return weighted_list({ type = "Blind", seed = "boss" })
  end
  local bosses1 = boss_list()
  check("bosses", function()
    return SMODS.poll_object({ type = "Blind", seed = "boss" })
  end, function(next)
    return pick_weighted(bosses1, next)
  end)
  trace("bosses per ante")
  for a = 1, MAX_ANTE do
    gm.round_resets.ante = a
    for _, e in ipairs(boss_list()) do
      if antes[e.key] then
        antes[e.key] = antes[e.key] .. a
        boss_weight[e.key] = boss_weight[e.key] or e.weight
      end
    end
  end
  gm.round_resets.ante = 1
  for _, key in ipairs(bosses) do
    add("boss", key, antes[key] ~= "" and antes[key] or "-", { raw = "*" .. number(boss_weight[key] or 10) })
  end

  -- Shop packs.
  if fresh_first_pack and not banned["p_buffoon_normal_1"] then
    add("firstpack", "p_buffoon_normal")
  end
  trace("packs")
  local packs = weighted_list({ type = "Booster", seed = "shop_pack1" })
  check("shop packs", function()
    local c = get_pack("shop_pack")
    return c and pack_kind(c.key)
  end, function(next)
    local key = pick_weighted(packs, next)
    return key and pack_kind(key)
  end)
  add("packlist", weighted_words(packs, nil, pack_kind))

  ---------------------------------------------------------------------------
  -- The shop: kinds of card and their rates (SMODS.poll_object_type), then
  -- the items of each kind.
  local kinds = { "Joker", "playing_card" }
  for _, v in ipairs(SMODS.ConsumableType.obj_buffer) do
    kinds[#kinds + 1] = v
  end
  local total = 0
  for _, kind in ipairs(kinds) do
    total = total + num(gm[kind:lower() .. "_rate"])
  end
  add("shop", number(total), number(num(gm.joker_rate)), tostring(gm.shop and gm.shop.joker_max or 2))

  -- Joker rarities, as in build(): how many rolls, which rarity per roll.
  trace("rarities")
  count_key, count = "rarity1sho", 0
  roll = 0.5
  get_current_pool("Joker", nil, nil, "sho")
  local rarity_polls = math.max(1, math.min(4, count))
  add("raritypolls", tostring(rarity_polls))
  count_key = nil
  -- The rarity roll itself is probed (SMODS.poll_rarity). It asks for every
  -- rarity's joker list on every call, only to see whether the list is
  -- empty; with several hundred modded jokers that is slow and piles up
  -- memory, and the answer can't change from one roll to the next, so it is
  -- remembered meanwhile.
  trace("rarity steps")
  local real_pool, emptiness = get_current_pool, {}
  get_current_pool = function(_type, _rarity, _legendary, _append)
    if _type == "Joker" and _rarity ~= nil and not _legendary and _append == nil then
      local known = emptiness[_rarity]
      if not known then
        local pool = real_pool(_type, _rarity, _legendary, _append)
        known = (#pool == 1 and pool[1] == "empty_rarity") and { "empty_rarity" } or { "", "" }
        emptiness[_rarity] = known
      end
      return known, ""
    end
    return real_pool(_type, _rarity, _legendary, _append)
  end
  local steps_ok, found = pcall(steps, function(x)
    roll = x
    return tostring(SMODS.poll_rarity("Joker", "rarity1sho"))
  end, 512)
  get_current_pool = real_pool
  if not steps_ok then
    error(found, 0)
  end
  -- (No rarity at all, right at the top of the rolls, is a rounding gap in
  -- the game's own sums; the game itself would fail there.)
  local rarity_steps = {}
  for _, s in ipairs(found) do
    if s.value ~= "nil" then
      rarity_steps[#rarity_steps + 1] = s
    end
  end
  if #rarity_steps == 0 or rarity_steps[1].from ~= 0 then
    error("can't tell how this game picks a joker's rarity", 0)
  end
  local function rarity_at(x)
    local i = #rarity_steps
    while i > 1 and x < rarity_steps[i].from do
      i = i - 1
    end
    return i
  end

  -- Every list whose weights can change with what is on offer, by name,
  -- with the function that builds it (under a fixed `roll` for jokers, so
  -- the game's own rarity roll lands on the rarity wanted).
  local makers, order = {}, {}
  local function maker(name, fn)
    makers[name] = fn
    order[#order + 1] = name
  end
  for i, s in ipairs(rarity_steps) do
    local upto = rarity_steps[i + 1] and rarity_steps[i + 1].from or 1
    local inside = s.from + (upto - s.from) / 2
    maker("rarity" .. i, function()
      roll = inside
      return members_of({ type = "Joker", append = "sho" })
    end)
  end
  local shop_kinds = {}
  for _, kind in ipairs(kinds) do
    if kind ~= "Joker" and kind ~= "playing_card" and num(gm[kind:lower() .. "_rate"]) > 0 then
      if SMODS.ObjectTypes[kind] and SMODS.ObjectTypes[kind].rarities then
        error("shop cards of kind " .. kind .. " have rarities, which the searcher can't express", 0)
      end
      shop_kinds[#shop_kinds + 1] = kind
      maker("kind" .. kind, function()
        return members_of({ type = kind, append = "sho", guaranteed = kind == "Enhanced" })
      end)
    end
  end
  maker("legendary", function()
    return members_of({ type = "Joker", guaranteed = true, rarities = { "Legendary" }, append = "sou" })
  end)
  -- Tarot, Planet and Spectral cards as the packs pick them (create_card:
  -- poll_object with the pack's key_append).
  local PACK_APPEND = { Tarot = "ar1", Planet = "pl1", Spectral = "spe" }
  local card_sets = {}
  for _, set in ipairs({ "Tarot", "Planet", "Spectral" }) do
    if G.P_CENTER_POOLS[set] then
      card_sets[#card_sets + 1] = set
      maker("cards" .. set, function()
        return members_of({ type = set, guaranteed = true, append = PACK_APPEND[set] })
      end)
    end
  end
  -- Every list, and (in `members`) the items each was weighed from.
  local members = {}
  local function build_all()
    local all = {}
    for _, name in ipairs(order) do
      members[name] = makers[name]()
      all[name] = weigh(members[name])
    end
    return all
  end

  -- With every "needs an enhanced card" gate open (the filter's `assume
  -- enhanced`), and as the game is, to tell which items are gated.
  local real_has = SMODS.has_enhancement
  trace("lists")
  local closed = build_all()
  if real_has then
    SMODS.has_enhancement = function()
      return true
    end
  end
  local ok, base = pcall(build_all)
  SMODS.has_enhancement = real_has
  if not ok then
    error(base, 0)
  end
  local gated = {}
  for _, name in ipairs(order) do
    local there = {}
    for _, e in ipairs(closed[name]) do
      there[e.key] = true
    end
    for _, e in ipairs(base[name]) do
      if not there[e.key] then
        gated[e.key] = true
      end
    end
  end

  -- The game's real picks against ours (on the lists as the game has them).
  trace("checks")
  local shop_stream = stream_of(function()
    return SMODS.poll_object({ type = "Joker", append = "sho" })
  end)
  local soul_stream = stream_of(function()
    return SMODS.poll_object({ type = "Joker", guaranteed = true, rarities = { "Legendary" }, append = "sou" })
  end)
  check("shop jokers", function()
    return SMODS.poll_object({ type = "Joker", append = "sho" })
  end, function(next)
    local i
    for _ = 1, rarity_polls do
      i = rarity_at(next())
    end
    return pick_weighted(closed["rarity" .. i], next)
  end)
  for _, kind in ipairs(shop_kinds) do
    check("shop cards of kind " .. kind, function()
      return SMODS.poll_object({ type = kind, append = "sho" })
    end, function(next)
      return pick_weighted(closed["kind" .. kind], next)
    end)
  end
  check("legendary jokers", function()
    return SMODS.poll_object({ type = "Joker", guaranteed = true, rarities = { "Legendary" }, append = "sou" })
  end, function(next)
    return pick_weighted(closed.legendary, next)
  end)
  -- Pack cards: the same picks, and the stream the searcher will use (the
  -- shop's, with the pack's key_append in place of "sho").
  for _, set in ipairs(card_sets) do
    local function real()
      return SMODS.poll_object({ type = set, guaranteed = true, append = PACK_APPEND[set] })
    end
    check(set .. " cards in packs", real, function(next)
      return pick_weighted(closed["cards" .. set], next)
    end)
    local expect = shop_stream:gsub("sho$", "") .. PACK_APPEND[set]
    if stream_of(real) ~= expect then
      error(set .. " cards in packs come from stream " .. tostring(stream_of(real)) .. ", not " .. expect, 0)
    end
  end
  add("streams", type_stream, shop_stream, soul_stream)

  for i, s in ipairs(rarity_steps) do
    add("rarity", number(s.from), "Joker" .. s.value .. "sho", weighted_words(base["rarity" .. i], gated))
  end
  for _, kind in ipairs(kinds) do
    local list = base["kind" .. kind]
    add("shoptype", kind, number(num(gm[kind:lower() .. "_rate"])), list and weighted_words(list, gated) or {})
  end

  -- Editions of shop jokers: one roll, as in build(); the stream is named
  -- after the key_append the shop passes to create_card.
  trace("editions")
  local edition_stream = "edi" .. shop_card_append("")
  add("editionstream", edition_stream)
  local edition_steps = steps(function(x)
    roll = x
    return edition_key(poll_edition(edition_stream .. "1"))
  end, 4096)
  for i, s in ipairs(edition_steps) do
    if i > 1 or s.value ~= "-" then
      if s.from == 0 then
        error("every shop joker gets an edition here, which the searcher can't express", 0)
      end
      add("edition", number(s.from), s.value)
    end
  end

  add("soul", (banned["c_soul"] or gm.used_jokers["c_soul"]) and "0" or "1")
  add("blackhole", (banned["c_soul"] or banned["c_black_hole"] or gm.used_jokers["c_black_hole"]) and "0" or "1")
  add("legendary", "Joker4", weighted_words(base.legendary, gated))
  for _, set in ipairs(card_sets) do
    add("cards", set, weighted_words(base["cards" .. set], gated))
  end
  add_modsouls(add)

  ---------------------------------------------------------------------------
  -- Links: what changes while one item is on offer. The game marks it used;
  -- every list is weighed again and compared. (The item itself always drops
  -- out; that isn't written.) Pokermon, for one, stops offering a Pokemon's
  -- relatives while it is in the shop.
  --
  -- Only the weights are worked out again, from the items found above:
  -- building each list from scratch for each of several hundred items would
  -- freeze the game for a long time. So an item that leaves or joins a list
  -- for another reason than its weight is not noticed.
  trace("links")
  if real_has then
    SMODS.has_enhancement = function()
      return true
    end
  end
  local marked -- the item marked used right now (unmarked again on an error)
  local started = love and love.timer and love.timer.getTime() or os.clock()
  local link_ok, link_err = pcall(function()
    local done, probed = {}, 0
    for _, name in ipairs(order) do
      for _, e in ipairs(base[name]) do
        local key = e.key
        if key ~= UNAVAILABLE and not done[key] and not gm.used_jokers[key] then
          done[key] = true
          probed = probed + 1
          if probed % 32 == 0 then
            local now = love and love.timer and love.timer.getTime() or os.clock()
            if now - started > GD.LINK_SECONDS then
              error("reading this modded game's item lists takes too long", 0)
            end
          end
          marked = key
          gm.used_jokers[key] = true
          local changed = {}
          for _, other in ipairs(order) do
            local now = weigh(members[other])
            for i, e2 in ipairs(base[other]) do
              if e2.key ~= key and e2.key ~= UNAVAILABLE and now[i].weight ~= e2.weight then
                changed[#changed + 1] = { key = e2.key, weight = now[i].weight }
              end
            end
          end
          gm.used_jokers[key] = nil
          marked = nil
          if #changed > 0 then
            add("link", key, weighted_words(changed))
          end
        end
      end
    end
  end)
  SMODS.has_enhancement = real_has
  if marked then
    gm.used_jokers[marked] = nil
  end
  if not link_ok then
    error(link_err, 0)
  end
  trace("done")

  return table.concat(lines, "\n") .. "\n"
end

-- The longest the link probe of a weighted game may take, in seconds; past
-- that the export fails (and the search is refused) instead of freezing
-- the game.
GD.LINK_SECONDS = 8
-- The longest reading the lists may take in all (see the watchdog in
-- sandbox()).
GD.MAX_SECONDS = 12

-- Text per cache_key(): { text = ..., fresh = bool }.
GD.cache = {}
-- Why the last export failed, or nil.
GD.last_error = nil

-- The game's lists as text, or nil and the reason. Cheap after the first
-- call for a deck/stake/challenge; taken again once the run is fresh if the
-- remembered copy was taken mid-run.
function GD.get()
  if not (G and G.GAME and G.GAME.round_resets and G.P_CENTER_POOLS and get_current_pool) then
    return nil, "the game isn't ready"
  end
  local key = GD.cache_key()
  local fresh = GD.is_fresh()
  local hit = GD.cache[key]
  if hit and (hit.fresh or not fresh) then
    return hit.text, hit.error
  end
  -- A fresh run still has its fixed first pack ahead; mid-run, the next run
  -- will have it again.
  local first_pack = not (fresh and G.GAME.first_shop_buffoon)
  local ok, result = sandbox(fresh, function()
    return build(fresh, first_pack)
  end)
  if ok then
    GD.cache[key] = { text = result, fresh = fresh }
    GD.last_error = nil
    return result
  end
  -- Remember the failure too (per run state), so it isn't retried on every
  -- key press; it is tried again when a fresh run comes along.
  local reason = tostring(result):match("^[^\n]*") or "unknown error"
  GD.cache[key] = { text = nil, error = reason, fresh = fresh }
  GD.last_error = reason
  print("[Brainstorm-Rerolled] could not read the game's item lists: " .. tostring(result))
  return nil, reason
end

return GD
