--[[
  mock_game.lua — a stand-in for the unmodded game, just enough of it to run
  Brainstorm/Core/gamedata.lua outside Balatro (tools/test-exporter.sh).

    luajit tools/mock_game.lua DUMP EXPORTER [midrun]

  DUMP is the built-in game data as text (gamedata_tool dump): the mock's
  item tables are built from it, so what the exporter reads back through the
  game's functions can be compared with it. The functions below follow the
  game's own (functions/common_events.lua): get_current_pool, get_pack,
  poll_edition. With "midrun" the run has bought vouchers, owns jokers and
  has changed shop rates, which the exporter must see through. With "modded"
  the game has items the library has never heard of.

  This checks the exporter's logic, not the real game, which was checked
  separately.
]]

local dump_path, exporter_path, mode = arg[1], arg[2], arg[3]

G = {
  STAGES = { RUN = 1, MAIN_MENU = 2 },
  STATES = { BLIND_SELECT = 7, SHOP = 5 },
  SETTINGS = { paused = true },
  ARGS = {},
  P_CENTERS = {},
  P_CENTER_POOLS = { Tag = {}, Voucher = {}, Booster = {}, Joker = {}, Tarot = {} },
  P_JOKER_RARITY_POOLS = { {}, {}, {}, {} },
  P_BLINDS = {},
}
G.STAGE, G.STATE = G.STAGES.RUN, G.STATES.BLIND_SELECT

function EMPTY(t)
  if not t then return {} end
  for k in pairs(t) do t[k] = nil end
  return t
end

local function center(pool, c)
  G.P_CENTERS[c.key] = c
  pool[#pool + 1] = c
  return c
end

local function strip(word) -- "key?" -> "key", "?"
  local mark = word:sub(-1)
  if mark == "?" or mark == "!" then return word:sub(1, -2), mark end
  return word, ""
end

-- Build the tables from the dump.
local tag_first_ante, tag_order = {}, {}
local pack_from, joker_rate, total_rate = {}, 20, 28
for line in io.lines(dump_path) do
  local w = {}
  for word in line:gmatch("%S+") do w[#w + 1] = word end
  if w[1] == "tags" then
    for i = 3, #w do
      local key, mark = strip(w[i])
      if not tag_first_ante[key] then tag_order[#tag_order + 1] = key; tag_first_ante[key] = 99 end
      if mark == "" then tag_first_ante[key] = math.min(tag_first_ante[key], tonumber(w[2])) end
    end
  elseif w[1] == "voucher" then
    local c = center(G.P_CENTER_POOLS.Voucher, { key = w[2], set = "Voucher", unlocked = true })
    if w[5] then c.requires = { w[5] } end
  elseif w[1] == "pack" then
    pack_from[#pack_from + 1] = { from = tonumber(w[2]), key = w[3] }
  elseif w[1] == "shop" then
    total_rate, joker_rate = tonumber(w[2]), tonumber(w[3])
  elseif w[1] == "rarity" then
    local rarity = #G.P_JOKER_RARITY_POOLS[1] == 0 and 1 or #G.P_JOKER_RARITY_POOLS[2] == 0 and 2 or 3
    for i = 4, #w do
      local key, mark = strip(w[i])
      local c = center(G.P_CENTER_POOLS.Joker, { key = key, set = "Joker", rarity = rarity, unlocked = true, name = key })
      if mark == "?" then c.enhancement_gate = "m_test" end
      if mark == "!" then c.yes_pool_flag = "never" end
      table.insert(G.P_JOKER_RARITY_POOLS[rarity], c)
    end
  elseif w[1] == "legendary" then
    for i = 3, #w do
      local c = center(G.P_CENTER_POOLS.Joker, { key = strip(w[i]), set = "Joker", rarity = 4, unlocked = false, name = w[i] })
      table.insert(G.P_JOKER_RARITY_POOLS[4], c)
    end
  elseif w[1] == "boss" then
    local showdown = w[3] == "8"
    G.P_BLINDS[w[2]] = { boss = { min = showdown and 10 or tonumber(w[3]:sub(1, 1)), showdown = showdown or nil } }
  end
end
for _, key in ipairs(tag_order) do
  local first = tag_first_ante[key]
  center(G.P_CENTER_POOLS.Tag, { key = key, set = "Tag", min_ante = first > 1 and first or nil })
end
-- One booster per pack kind, with the weight its share of the rolls implies
-- (the real game has several artworks per kind: "_1", "_2", ...).
local TOTAL_WEIGHT = 22.42
for i, p in ipairs(pack_from) do
  local to = pack_from[i + 1] and pack_from[i + 1].from or 1
  center(G.P_CENTER_POOLS.Booster, { key = p.key .. "_1", set = "Booster", weight = (to - p.from) * TOTAL_WEIGHT })
end
for _, key in ipairs({ "c_fool", "c_magician", "c_high_priestess" }) do
  center(G.P_CENTER_POOLS.Tarot, { key = key, set = "Tarot", unlocked = true, name = key })
end
for _, key in ipairs({ "e_foil", "e_holo", "e_polychrome", "e_negative" }) do
  G.P_CENTERS[key] = { key = key, set = "Edition" }
end

if mode == "modded" then
  -- What a content mod adds: a joker, a tag, a voucher pair, a pack, a boss.
  local j = center(G.P_CENTER_POOLS.Joker, { key = "j_mod_one", set = "Joker", rarity = 1, unlocked = true, name = "j_mod_one" })
  table.insert(G.P_JOKER_RARITY_POOLS[1], j)
  center(G.P_CENTER_POOLS.Tag, { key = "tag_mod", set = "Tag" })
  center(G.P_CENTER_POOLS.Voucher, { key = "v_mod_base", set = "Voucher", unlocked = true })
  center(G.P_CENTER_POOLS.Voucher, { key = "v_mod_plus", set = "Voucher", unlocked = true, requires = { "v_mod_base" } })
  center(G.P_CENTER_POOLS.Booster, { key = "p_mod_pack_1", set = "Booster", weight = 3 })
  G.P_BLINDS.bl_mod = { boss = { min = 1 } }
end

function Game_init_game_object()
  return {
    joker_rate = joker_rate, tarot_rate = 4, planet_rate = 4, spectral_rate = 0,
    playing_card_rate = total_rate - joker_rate - 8, edition_rate = 1,
  }
end
G.init_game_object = function(self) return Game_init_game_object() end

G.GAME = Game_init_game_object()
local gm = G.GAME
gm.round_resets = { ante = 1, blind_states = { Small = "Select" }, blind_choices = { Boss = "bl_hook" } }
gm.round, gm.stake, gm.win_ante = 0, 1, 8
gm.used_vouchers, gm.used_jokers, gm.pool_flags, gm.banned_keys = {}, {}, {}, {}
gm.pseudorandom = { seed = "MOCKSEED", hashed_seed = 0.25 }
gm.selected_back = { name = "Red Deck", effect = { config = {} } }
gm.shop = { joker_max = 2 }
G.playing_cards = {}
G.jokers = { cards = {} }

if mode == "midrun" then
  -- Third shop of ante 3, with things bought along the way.
  G.STATE = G.STATES.SHOP
  gm.round_resets.ante, gm.round = 3, 7
  gm.round_resets.blind_states.Small = "Defeated"
  gm.first_shop_buffoon = true
  gm.used_vouchers = { v_telescope = true, v_tarot_merchant = true, v_hone = true }
  gm.used_jokers = { j_joker = true, j_blueprint = true, c_fool = true }
  gm.pool_flags = { never = true }
  gm.tarot_rate, gm.edition_rate = 9.6, 2
  G.shop_vouchers = { cards = { { config = { center = { key = "v_grabber" } } } } }
end

function find_joker() return {} end

function pseudoseed() error("the exporter must not draw from the run's random streams") end
function pseudorandom() error("the exporter must not draw from the run's random streams") end

function get_current_pool(_type, _rarity, _legendary, _append)
  G.ARGS.TEMP_POOL = EMPTY(G.ARGS.TEMP_POOL)
  local _pool, _starting_pool, _pool_key, _pool_size = G.ARGS.TEMP_POOL, nil, "", 0
  if _type == "Joker" then
    local rarity = _rarity or pseudorandom("rarity" .. G.GAME.round_resets.ante .. (_append or ""))
    rarity = (_legendary and 4) or (rarity > 0.95 and 3) or (rarity > 0.7 and 2) or 1
    _starting_pool, _pool_key = G.P_JOKER_RARITY_POOLS[rarity], "Joker" .. rarity .. ((not _legendary and _append) or "")
  else
    _starting_pool, _pool_key = G.P_CENTER_POOLS[_type], _type .. (_append or "")
  end
  for _, v in ipairs(_starting_pool) do
    local add = nil
    if _type == "Tag" then
      if not v.min_ante or v.min_ante <= G.GAME.round_resets.ante then add = true end
    elseif not (G.GAME.used_jokers[v.key] and not next(find_joker("Showman"))) and (v.unlocked ~= false or v.rarity == 4) then
      if v.set == "Voucher" then
        if not G.GAME.used_vouchers[v.key] then
          local include = true
          if v.requires then
            for _, vv in pairs(v.requires) do
              if not G.GAME.used_vouchers[vv] then include = false end
            end
          end
          if G.shop_vouchers and G.shop_vouchers.cards then
            for _, vv in ipairs(G.shop_vouchers.cards) do
              if vv.config.center.key == v.key then include = false end
            end
          end
          if include then add = true end
        end
      elseif v.enhancement_gate then
        add = nil
        for _, vv in pairs(G.playing_cards) do
          if vv.config.center.key == v.enhancement_gate then add = true end
        end
      else
        add = true
      end
    end
    if v.no_pool_flag and G.GAME.pool_flags[v.no_pool_flag] then add = nil end
    if v.yes_pool_flag and not G.GAME.pool_flags[v.yes_pool_flag] then add = nil end
    if add and not G.GAME.banned_keys[v.key] then
      _pool[#_pool + 1] = v.key
      _pool_size = _pool_size + 1
    else
      _pool[#_pool + 1] = "UNAVAILABLE"
    end
  end
  return _pool, _pool_key .. (not _legendary and G.GAME.round_resets.ante or "")
end

function get_pack(_key, _type)
  if not G.GAME.first_shop_buffoon and not G.GAME.banned_keys["p_buffoon_normal_1"] then
    G.GAME.first_shop_buffoon = true
    return G.P_CENTERS["p_buffoon_normal_1"]
  end
  local cume, it, c = 0, 0, nil
  for _, v in ipairs(G.P_CENTER_POOLS["Booster"]) do
    if (not _type or _type == v.kind) and not G.GAME.banned_keys[v.key] then cume = cume + (v.weight or 1) end
  end
  local poll = pseudorandom(pseudoseed((_key or "pack_generic") .. G.GAME.round_resets.ante)) * cume
  for _, v in ipairs(G.P_CENTER_POOLS["Booster"]) do
    if not G.GAME.banned_keys[v.key] then
      if not _type or _type == v.kind then it = it + (v.weight or 1) end
      if it >= poll and it - (v.weight or 1) <= poll then c = v; break end
    end
  end
  return c
end

function poll_edition(_key, _mod, _no_neg, _guaranteed)
  _mod = _mod or 1
  local edition_poll = pseudorandom(pseudoseed(_key or "edition_generic"))
  if edition_poll > 1 - 0.003 * _mod and not _no_neg then
    return { negative = true }
  elseif edition_poll > 1 - 0.006 * G.GAME.edition_rate * _mod then
    return { polychrome = true }
  elseif edition_poll > 1 - 0.02 * G.GAME.edition_rate * _mod then
    return { holo = true }
  elseif edition_poll > 1 - 0.04 * G.GAME.edition_rate * _mod then
    return { foil = true }
  end
  return nil
end

local before = { ante = gm.round_resets.ante, pr = gm.pseudorandom, uv = gm.used_vouchers, paused = G.SETTINGS.paused, tarot = gm.tarot_rate }
local GD = assert(loadfile(exporter_path))()
local text, err = GD.get()
if not text then
  io.stderr:write("exporter failed: " .. tostring(err) .. "\n")
  os.exit(1)
end
-- The run must be exactly as it was.
assert(gm.round_resets.ante == before.ante and gm.pseudorandom == before.pr and gm.used_vouchers == before.uv, "run state not restored")
assert(G.SETTINGS.paused == before.paused and gm.tarot_rate == before.tarot, "settings not restored")
assert(gm.round_resets.blind_choices.Boss == "bl_hook", "blind choices not restored")
assert(not pcall(pseudoseed, "x"), "pseudoseed not restored")
assert(GD.get() == text, "second call must come from the cache")
io.write(text)

-- With the mod's native bridge and a built library (arguments 4 and 5): the
-- exported text goes through the real C API, as in the game.
local native_path, lib_dir = arg[4], arg[5]
if native_path and lib_dir then
  local native = assert(loadfile(native_path))()
  assert(native.load(lib_dir))
  local function note(...) io.stderr:write(string.format(...) .. "\n") end
  local function search(spec)
    local opts = { start_seed = "MOCK1111", filter_spec = spec, game_data = text }
    local job = assert(native.start(opts))
    local status, seed
    local deadline = os.time() + 120
    repeat
      status, seed = native.poll(job)
    until status ~= "running" or os.time() > deadline
    native.free(job)
    assert(status == "found", "search did not finish: " .. tostring(status))
    assert(native.check_seed(opts, seed) == true, "found seed fails its own filter")
    return seed, assert(native.predict_start(opts, seed))
  end
  if mode == "modded" then
    -- Keys the library has never heard of are found like any other...
    local seed, start = search("tag tag_mod blind=small\nvoucher v_mod_base\nboss bl_mod")
    assert(start.small == "tag_mod" and start.voucher == "v_mod_base" and start.boss == "bl_mod", "bad start prediction")
    search("voucher v_mod_base\nvoucher v_mod_plus ante=2\npack p_mod_pack within=2\njoker j_mod_one within=6")
    -- ...and are unknown without the game's lists.
    local ok, err = native.check_seed({ filter_spec = "joker j_mod_one" }, seed)
    assert(ok == nil and err:find("unknown joker 'j_mod_one'", 1, true), tostring(err))
    note("native: modded filter found %s", seed)
    return
  end
  local seed, start = search("tag tag_charm blind=small\nsoul")
  local opts = { filter_spec = "tag tag_charm blind=small\nsoul", game_data = text }
  -- The same filter on the built-in lists must agree (the mock IS the unmodded game).
  assert(native.check_seed({ filter_spec = opts.filter_spec }, seed) == true, "built-in lists disagree")
  assert(start.small == "tag_charm" and start.big and start.voucher and start.boss, "bad start prediction")
  local plain = assert(native.predict_start({}, seed))
  assert(plain.small == start.small and plain.big == start.big and plain.voucher == start.voucher and plain.boss == start.boss)
  -- An impossible condition and broken game data are errors, not searches.
  local ok, err = native.check_seed({ filter_spec = "tag tag_negative", game_data = text }, seed)
  assert(ok == nil and err:find("can't be a skip tag", 1, true), tostring(err))
  ok, err = native.start({ filter_spec = "soul", game_data = "nonsense" })
  assert(ok == nil and err:find("game data, line 1", 1, true), tostring(err))
  note("native: found %s (small=%s big=%s voucher=%s boss=%s)", seed, start.small, start.big, start.voucher, start.boss)
end
