--[[
  filter_builder.lua — saved filters and the in-game filter builder.

  A filter is a named list of conditions that a seed must ALL satisfy, e.g.
    "Joker in shop: Blueprint, ante 1, first 4 cards, Negative"
    "Tag: Charm Tag, ante 1, Small Blind"
  Any number of filters are saved in Brainstorm.config.filters; one of them
  is "active" (chosen in Settings > Brainstorm-Rerolled, or with "Use" below), and
  that one is sent to the native search as text (Brainstorm.FB.spec()). The
  text format and the exact meaning of every condition are documented in
  Immolate/src/filterprog.hpp; this file only builds that text.

  Screens. They all live in ONE window: moving between them swaps the
  window's contents in place (like the game's settings tabs), instead of
  opening a new window each time, so nothing slides around.
    filters  the saved filters: use, edit, delete, create.
    filter   one filter: its name, the "assume" options, and its
             conditions (add, edit, remove).
    editor   one condition. On the left the item to choose (a grid of cards
             or chips like the Collection screen, with a search box); on the
             right the condition's options as rows of buttons, and Save.

  All item lists come from the game's data (G.P_CENTER_POOLS) and all names
  from localize(), so they are in the player's language.
]]

local FB = {}
Brainstorm.FB = FB

FB.MAX_FILTERS = 10
FB.MAX_CONDITIONS = 12

-- ---------------------------------------------------------------------------
-- What can be chosen

FB.KINDS = { "tag", "voucher", "pack", "joker", "soul", "card", "boss" }
FB.ADD_LABELS = { tag = "Tag", voucher = "Voucher", pack = "Pack", joker = "Joker", soul = "Soul", card = "Consumable", boss = "Boss" }
FB.KIND_LABELS = {
  tag = "Tag",
  voucher = "Voucher",
  pack = "Booster pack",
  joker = "Joker",
  soul = "The Soul",
  card = "Consumable",
  boss = "Boss blind",
}

FB.ANTES = { 1, 2, 3, 4, 5, 6, 7, 8 }
FB.BLINDS = { "any", "small", "big" }
FB.BLIND_LABELS = { "Either blind", "Small Blind", "Big Blind" }
FB.PACK_WITHIN = { 1, 2, 3, 4, 6 }
FB.JOKER_WITHIN = { 1, 2, 4, 6, 8, 10, 15, 20, 30, 50, 100, 150 }
-- The editions a shop joker can be asked to have: "any", "e_base" (none),
-- then the game's own, mods included (FB.editions(), built on first use).
local edition_list
function FB.editions()
  if not edition_list then
    edition_list = { "any", "e_base" }
    -- With Steamodded an edition says whether shops can have it (in_shop);
    -- the unmodded game has no such field, and all four of its editions
    -- can come up. Listed in the game's own order (Foil ... Negative).
    local found = {}
    for _, v in ipairs(G.P_CENTER_POOLS.Edition or {}) do
      if v.key ~= "e_base" and (v.in_shop or not SMODS) then
        found[#found + 1] = v
      end
    end
    table.sort(found, function(a, b)
      return (a.order or 0) < (b.order or 0)
    end)
    for _, v in ipairs(found) do
      edition_list[#edition_list + 1] = v.key
    end
  end
  return edition_list
end
-- The packs a Soul condition can open. (Two Souls in one ante: two Soul
-- conditions; each takes the next pack.)
FB.SOUL_FROM = { "arcana", "spectral" }
FB.SOUL_FROM_LABELS = { "Arcana", "Spectral" }
-- Where a joker condition looks: the shop, or Buffoon packs.
FB.JOKER_FROM = { "shop", "buffoon" }
FB.JOKER_FROM_LABELS = { "Shop", "Buffoon pack" }
FB.BUFFOON_PACKS = { 1, 2, 3, 4 }
-- Where a card condition (Tarot / Planet / Spectral) looks.
FB.CARD_FROM_LABELS = { shop = "Shop", arcana = "Arcana pack", celestial = "Celestial pack", spectral = "Spectral pack" }
FB.CARD_PACK_NAMES = { arcana = "Arcana", celestial = "Celestial", spectral = "Spectral" }
-- Where a card can come from: its own kind of pack, or the shop. The Soul
-- and Black Hole only come from the rolls packs make for them.
function FB.card_froms(key)
  -- A mod's Soul-like card (Steamodded): the packs whose cards roll for it.
  local mods = SMODS and SMODS.Consumable and SMODS.Consumable.legendaries
  for _, v in ipairs(type(mods) == "table" and mods or {}) do
    if v.key == key then
      local froms = {}
      for _, pair in ipairs({ { "Tarot", "arcana" }, { "Planet", "celestial" }, { "Spectral", "spectral" } }) do
        if (v.type and v.type.key) == pair[1] or v.soul_set == pair[1] then
          froms[#froms + 1] = pair[2]
        end
      end
      if #froms > 0 then
        return froms
      end
    end
  end
  if key == "c_soul" then
    return { "arcana", "spectral" }
  elseif key == "c_black_hole" then
    return { "celestial", "spectral" }
  end
  local set = G.P_CENTERS[key] and G.P_CENTERS[key].set
  local pack = ({ Tarot = "arcana", Planet = "celestial", Spectral = "spectral" })[set] or "arcana"
  return { pack, "shop" }
end
-- Pack sizes per kind of pack, the most common first.
FB.SIZES = { arcana = { 5, 3 }, celestial = { 5, 3 }, spectral = { 4, 2 }, buffoon = { 2, 4 } }
FB.SIZE_LABELS = {
  arcana = { "5 cards (Jumbo, Mega)", "3 cards (Normal)" },
  celestial = { "5 cards (Jumbo, Mega)", "3 cards (Normal)" },
  spectral = { "4 cards (Jumbo, Mega)", "2 cards (Normal)" },
  buffoon = { "2 cards (Normal)", "4 cards (Jumbo, Mega)" },
}

local function name_of(set, key)
  local ok, name = pcall(localize, { type = "name_text", set = set, key = key })
  if ok and type(name) == "string" and name ~= "ERROR" then
    return name
  end
  return key
end

local function edition_label(key)
  if key == "any" then
    return "Any edition"
  elseif key == "e_base" then
    return "No edition"
  end
  return name_of("Edition", key)
end

-- Items for a kind, as a list of { key = ..., label = ... }, in the game's
-- own collection order. Built on first use (the game data must be loaded).
local item_cache = {}
function FB.items(kind)
  if item_cache[kind] then
    return item_cache[kind]
  end
  local list = {}
  if kind == "tag" then
    for _, v in ipairs(G.P_CENTER_POOLS.Tag) do
      list[#list + 1] = { key = v.key, label = name_of("Tag", v.key) }
    end
  elseif kind == "voucher" then
    for _, v in ipairs(G.P_CENTER_POOLS.Voucher) do
      list[#list + 1] = { key = v.key, label = name_of("Voucher", v.key) }
    end
  elseif kind == "pack" then
    -- "p_arcana_normal_1" .. "_4" only differ in artwork: one entry each.
    local seen = {}
    for _, v in ipairs(G.P_CENTER_POOLS.Booster) do
      local base = v.key:match("^(.*)_%d+$") or v.key
      if not seen[base] then
        seen[base] = true
        list[#list + 1] = { key = base, label = name_of("Other", base) }
      end
    end
  elseif kind == "joker" then
    -- Legendary jokers only come from The Soul (use the "soul" kind), and
    -- Cavendish only after Gros Michel dies, which can't be searched for.
    -- The five jokers that need an enhanced card in the deck are listed;
    -- they need "Assume 1 of each enhanced card" (see FB.is_gated).
    for _, v in ipairs(G.P_CENTER_POOLS.Joker) do
      if v.rarity ~= 4 and not v.yes_pool_flag then
        list[#list + 1] = { key = v.key, label = name_of("Joker", v.key) }
      end
    end
  elseif kind == "card" then
    -- Tarot, Planet and Spectral cards (The Soul and Black Hole included:
    -- packs roll for them), in the Collection's order.
    for _, set in ipairs({ "Tarot", "Planet", "Spectral" }) do
      for _, v in ipairs(G.P_CENTER_POOLS[set] or {}) do
        list[#list + 1] = { key = v.key, label = name_of(set, v.key) }
      end
    end
  elseif kind == "boss" then
    -- Regular bosses by the ante they first appear on, then the five
    -- final bosses (ante 8 only); the native side rejects a boss on an
    -- ante where the game never picks it.
    local bosses = {}
    for k, v in pairs(G.P_BLINDS) do
      if v.boss then
        bosses[#bosses + 1] = { key = k, order = (v.boss.showdown and 100 or v.boss.min) * 1000 + (v.order or 0) }
      end
    end
    table.sort(bosses, function(a, b)
      return a.order < b.order
    end)
    for _, b in ipairs(bosses) do
      list[#list + 1] = { key = b.key, label = name_of("Blind", b.key) }
    end
  elseif kind == "legendary" then
    list[1] = { key = "any", label = "Any legendary" }
    for _, v in ipairs(G.P_CENTER_POOLS.Joker) do
      if v.rarity == 4 then
        list[#list + 1] = { key = v.key, label = name_of("Joker", v.key) }
      end
    end
  end
  item_cache[kind] = list
  return list
end

local function label_of(kind, key)
  for _, it in ipairs(FB.items(kind)) do
    if it.key == key then
      return it.label
    end
  end
  return tostring(key)
end

-- True for jokers the game only offers once a card with a certain
-- enhancement is in the deck (Steel Joker, Stone Joker, Lucky Cat, Golden
-- Ticket, Glass Joker).
function FB.is_gated(key)
  local center = G.P_CENTERS[key]
  return center and center.enhancement_gate ~= nil
end

-- ---------------------------------------------------------------------------
-- Conditions and filters: defaults, text for the native search, text for
-- people

function FB.new_condition(kind)
  local c = { kind = kind, ante = 1, ante_to = 1 }
  if kind == "tag" then
    c.key, c.blind = "tag_charm", "any"
  elseif kind == "voucher" then
    c.key = "v_telescope"
  elseif kind == "pack" then
    c.key, c.within = "p_arcana_mega", 2
  elseif kind == "joker" then
    c.key, c.within, c.edition, c.from, c.packs, c.size = "j_blueprint", 4, "any", "shop", 1, 2
  elseif kind == "soul" then
    c.size, c.legendary, c.edition, c.from = 5, "any", "any", "arcana"
  elseif kind == "card" then
    c.key, c.from, c.packs, c.size, c.within = "c_temperance", "arcana", 1, 5, 4
  elseif kind == "boss" then
    c.key = "bl_hook"
  end
  return c
end

local function ante_spec(c)
  local to = c.ante_to or c.ante
  return to > c.ante and (c.ante .. "-" .. to) or tostring(c.ante)
end

-- One line of the native text format (Immolate/src/filterprog.hpp).
function FB.condition_spec(c)
  local ante = ante_spec(c)
  -- Keys are percent-encoded: a modded key may contain a space.
  local enc = Brainstorm.gamedata and Brainstorm.gamedata.encode or tostring
  c = setmetatable({ key = c.key and enc(c.key), legendary = c.legendary and enc(c.legendary),
    edition = c.edition and enc(c.edition) }, { __index = c })
  if c.kind == "tag" then
    return string.format("tag %s ante=%s blind=%s", c.key, ante, c.blind)
  elseif c.kind == "voucher" then
    return string.format("voucher %s ante=%s", c.key, ante)
  elseif c.kind == "pack" then
    return string.format("pack %s ante=%s within=%d", c.key, ante, c.within)
  elseif c.kind == "joker" and c.from == "buffoon" then
    return string.format("joker %s ante=%s from=buffoon packs=%d size=%d edition=%s", c.key, ante, c.packs, c.size, c.edition)
  elseif c.kind == "joker" then
    return string.format("joker %s ante=%s within=%d edition=%s", c.key, ante, c.within, c.edition)
  elseif c.kind == "soul" then
    return string.format("soul ante=%s size=%d legendary=%s edition=%s from=%s", ante, c.size, c.legendary, c.edition, c.from)
  elseif c.kind == "card" and c.from == "shop" then
    return string.format("card %s ante=%s from=shop within=%d", c.key, ante, c.within)
  elseif c.kind == "card" then
    return string.format("card %s ante=%s from=%s packs=%d size=%d", c.key, ante, c.from, c.packs, c.size)
  elseif c.kind == "boss" then
    return string.format("boss %s ante=%s", c.key, ante)
  end
  return ""
end

local function ordinal(n)
  return n .. (({ "st", "nd", "rd" })[n] or "th")
end

-- A short description for the condition list. nth: for the second (third,
-- ...) Soul condition on the same antes, which Soul of them it is.
function FB.describe(c, nth)
  local ante = "Ante " .. ante_spec(c)
  if c.kind == "tag" then
    local blind = ({ any = "either blind", small = "Small Blind", big = "Big Blind" })[c.blind]
    return string.format("%s: %s, %s", ante, label_of("tag", c.key), blind)
  elseif c.kind == "voucher" then
    return string.format("%s: voucher %s", ante, label_of("voucher", c.key))
  elseif c.kind == "pack" then
    return string.format("%s: %s in first %d packs", ante, label_of("pack", c.key), c.within)
  elseif c.kind == "joker" then
    local ed = c.edition == "any" and "" or (" (" .. edition_label(c.edition) .. ")")
    if c.from == "buffoon" then
      local packs = c.packs == 1 and "the first Buffoon pack" or ("the first " .. c.packs .. " Buffoon packs")
      return string.format("%s: %s%s in %s (%d cards)", ante, label_of("joker", c.key), ed, packs, c.size)
    end
    return string.format("%s: %s%s in first %d shop cards", ante, label_of("joker", c.key), ed, c.within)
  elseif c.kind == "soul" then
    local leg = c.legendary == "any" and "The Soul" or ("Soul -> " .. label_of("legendary", c.legendary))
    local kind = c.from == "spectral" and "Spectral" or "Arcana"
    local pack = nth and ("the " .. ordinal(nth) .. " " .. kind .. " pack") or ("a" .. (kind == "Arcana" and "n " or " ") .. kind .. " pack")
    local ed = c.edition == "any" and "" or (" (" .. edition_label(c.edition) .. ")")
    return string.format("%s: %s%s in %s", ante, leg, ed, pack)
  elseif c.kind == "card" then
    local name = label_of("card", c.key)
    if c.from == "shop" then
      return string.format("%s: %s in first %d shop cards", ante, name, c.within)
    end
    local kind = FB.CARD_PACK_NAMES[c.from] or c.from
    local packs = c.packs == 1 and ("the first " .. kind .. " pack") or ("the first " .. c.packs .. " " .. kind .. " packs")
    return string.format("%s: %s in %s (%d cards)", ante, name, packs, c.size)
  elseif c.kind == "boss" then
    return string.format("%s: boss %s", ante, label_of("boss", c.key))
  end
  return "?"
end

-- A filter as native text.
function FB.filter_spec(f)
  local lines = {}
  if f.assume_enhanced then
    lines[#lines + 1] = "assume enhanced"
  end
  if f.assume_showman then
    lines[#lines + 1] = "assume showman"
  end
  for _, c in ipairs(f.conditions) do
    lines[#lines + 1] = FB.condition_spec(c)
  end
  return table.concat(lines, "\n")
end

function FB.active()
  local fs = Brainstorm.config.filters
  return fs and fs.list[fs.active] or nil
end

-- The active filter as native text ("" = none: every seed matches).
function FB.spec()
  local f = FB.active()
  return f and FB.filter_spec(f) or ""
end

-- Asks the native library whether a filter is valid. Returns nil if it is,
-- else the problem.
-- True in a game with Steamodded's object weights (Pokermon, ...) outside a
-- run: its item lists can only be read during a run (Core/gamedata.lua), so
-- filters can't be made or changed until then (the builder says so).
function FB.needs_run()
  local weighted = SMODS and SMODS.optional_features and SMODS.optional_features.object_weights
  return weighted and G.STAGE ~= G.STAGES.RUN or false
end

function FB.problem(f)
  if not (Brainstorm.native and Brainstorm.native.lib) then
    return nil
  end
  -- Checked against the running game's own item lists, so a modded joker or
  -- tag is known (see Core/gamedata.lua).
  local game_data = Brainstorm.gameData and Brainstorm.gameData() or nil
  -- A game with Steamodded's object weights (Pokermon, ...) can only have
  -- its lists read during a run, and the builder only edits filters then
  -- (FB.needs_run). Should the lists still be missing, the check would use
  -- the unmodded lists and reject every modded item, so it is skipped: the
  -- search itself, which needs a run, checks the filter again.
  if not game_data and SMODS and SMODS.optional_features and SMODS.optional_features.object_weights then
    return nil
  end
  local ok, err = Brainstorm.native.check_seed({
    filter_spec = FB.filter_spec(f),
    game_data = game_data,
  }, "1")
  if ok == nil then
    return tostring(err)
  end
  return nil
end

-- ---------------------------------------------------------------------------
-- Saved filters: starter filters and upgrading older settings

local function cond(t)
  local c = FB.new_condition(t.kind)
  for k, v in pairs(t) do
    c[k] = v
  end
  -- new_condition's ante_to is 1: without an ante_to of its own, a
  -- condition for a later ante holds in that ante only.
  c.ante_to = t.ante_to or c.ante
  return c
end

-- The filters a new install starts with. "Round 1" is ante 1's first round:
-- the Charm Tag is on the Small Blind (its Mega Arcana pack is the first
-- Arcana pack of the ante, which the Soul condition looks at), and the
-- first shop's first pack is always a normal Buffoon pack.
function FB.starter_filters()
  local function charm_perkeo(edition)
    return {
      cond({ kind = "tag", key = "tag_charm", blind = "small" }),
      cond({ kind = "soul", legendary = "j_perkeo", edition = edition or "any" }),
    }
  end
  local observatory = charm_perkeo()
  observatory[#observatory + 1] = cond({ kind = "voucher", key = "v_telescope" })
  observatory[#observatory + 1] = cond({ kind = "voucher", key = "v_observatory", ante = 2 })
  return {
    { name = "Charm + Perkeo", conditions = charm_perkeo() },
    { name = "Charm + Negative Perkeo", conditions = charm_perkeo("e_negative") },
    { name = "Charm + Perkeo + Observatory", conditions = observatory },
    { name = "Buffoon + Neg. Blueprint", conditions = {
      cond({ kind = "joker", key = "j_blueprint", from = "buffoon", packs = 1, size = 2, edition = "e_negative" }),
    } },
  }
end

-- The settings of the old built-in menu (config.ar_filters) as a filter.
-- ("Early copy + money" has no condition form and is left out.)
local function from_builtin(f)
  if type(f) ~= "table" then
    return nil
  end
  local list = {}
  if f.tag_name and f.tag_name ~= "" then
    list[#list + 1] = cond({ kind = "tag", key = f.tag_name, blind = "small" })
  end
  if f.inst_observatory then
    list[#list + 1] = cond({ kind = "voucher", key = "v_telescope" })
    list[#list + 1] = cond({ kind = "voucher", key = "v_observatory", ante = 2 })
  elseif f.voucher_name and f.voucher_name ~= "" then
    list[#list + 1] = cond({ kind = "voucher", key = f.voucher_name })
  end
  if type(f.pack) == "table" and f.pack[1] then
    list[#list + 1] = cond({ kind = "pack", key = f.pack[1]:match("^(.*)_%d+$") or f.pack[1] })
  end
  local souls = math.min(tonumber(f.soul_skip) or 0, 3)
  for _ = 1, souls do
    list[#list + 1] = cond({ kind = "soul", legendary = f.inst_perkeo and "j_perkeo" or "any" })
  end
  if f.bean then
    list[#list + 1] = cond({ kind = "joker", key = "j_turtle_bean", ante = 8, within = 150 })
  end
  if f.burglar then
    list[#list + 1] = cond({ kind = "joker", key = "j_burglar", ante = 6, ante_to = 8, within = 100 })
  end
  if f.retcon then
    list[#list + 1] = cond({ kind = "voucher", key = "v_retcon", ante = 1, ante_to = 8 })
  end
  if #list == 0 then
    return nil
  end
  -- Perkeo from two Souls needs a Showman.
  return { name = "Old settings", conditions = list, assume_showman = f.inst_perkeo and souls >= 2 }
end

-- Makes sure Brainstorm.config.filters exists and is well-formed. The first
-- time, it is filled from older settings (the single custom filter of the
-- first builder version, and the old built-in menu) plus the starter filters.
function FB.ensure_config()
  local cfg = Brainstorm.config
  if type(cfg.filters) ~= "table" or type(cfg.filters.list) ~= "table" then
    local fs = { active = 0, list = {} }
    local old = cfg.custom_filter
    if type(old) == "table" and type(old.conditions) == "table" and #old.conditions > 0 then
      fs.list[#fs.list + 1] = { name = "My filter", assume_enhanced = old.assume_enhanced, conditions = old.conditions }
      if old.enabled then
        fs.active = #fs.list
      end
    end
    local builtin = from_builtin(cfg.ar_filters)
    if builtin then
      fs.list[#fs.list + 1] = builtin
      if fs.active == 0 then
        fs.active = #fs.list
      end
    end
    for _, f in ipairs(FB.starter_filters()) do
      fs.list[#fs.list + 1] = f
    end
    cfg.filters = fs
    -- Older settings, now part of `filters`.
    cfg.custom_filter = nil
    cfg.ar_filters = nil
    cfg.ar_prefs = nil
  end
  local fs = cfg.filters
  for _, f in ipairs(fs.list) do
    f.name = type(f.name) == "string" and f.name or "Filter"
    f.assume_enhanced = f.assume_enhanced and true or false
    f.assume_showman = f.assume_showman and true or false
    f.conditions = type(f.conditions) == "table" and f.conditions or {}
    local list = {}
    for _, c in ipairs(f.conditions) do
      -- (Starter filters of earlier versions could have an ante_to below
      -- their ante: the same single ante, shown wrongly in the editor.)
      if not c.ante_to or c.ante_to < c.ante then
        c.ante_to = c.ante
      end
      list[#list + 1] = c
      if c.kind == "soul" then
        -- Saved before Soul conditions had an edition or a kind of pack.
        c.edition = c.edition or "any"
        c.from = c.from or "arcana"
        -- Saved with "packs in a row": one condition per Soul now (each
        -- Soul condition on an ante takes the next pack).
        for _ = 2, tonumber(c.packs) or 1 do
          local again = copy_table(c)
          again.packs = nil
          list[#list + 1] = again
        end
        c.packs = nil
      elseif c.kind == "card" then
        c.from = c.from or "arcana"
        c.packs, c.size, c.within = c.packs or 1, c.size or 5, c.within or 4
      elseif c.kind == "joker" then
        -- Saved before jokers could be looked for in Buffoon packs.
        c.from = c.from or "shop"
        c.packs = c.packs or 1
        c.size = c.size or 2
      end
    end
    f.conditions = list
  end
  if type(fs.active) ~= "number" or not fs.list[fs.active] then
    fs.active = 0
  end
end

-- ---------------------------------------------------------------------------
-- The window and moving between screens

FB.screen = nil -- "filter" or "editor" (the filter list is in Settings, UI/settings.lua)
FB.filter_index = nil -- the filter being edited
FB.draft = nil -- the condition being edited (a copy; saved on "Save")
FB.draft_index = nil -- its position in the filter, or nil for a new one
FB.pick_kind = nil -- editor: what is being chosen (a kind, or "legendary")
FB.range_from = nil -- editor: first ante clicked of a range, waiting for the last
FB.pick_page = 1
FB.search = "" -- the picker's search box
FB.search_applied = ""
FB.confirm_delete = nil -- filters screen: index waiting for a second click

local screens = {} -- name -> function returning the screen's nodes

local function body(name)
  return { n = G.UIT.ROOT, config = { align = "cm", colour = G.C.CLEAR }, nodes = screens[name]() }
end

-- Shows a screen. If the filter window is open, only its contents are
-- replaced (as G.FUNCS.change_tab does), so nothing moves; otherwise the
-- window is opened. Done one frame later, so it is safe to call from a
-- button or option cycle that is part of the contents being replaced.
function FB.go(name)
  G.E_MANAGER:add_event(Event({
    blockable = false,
    blocking = false,
    timer = "REAL",
    func = function()
      -- Finish typing in a text box that is about to disappear.
      if G.CONTROLLER.text_input_hook then
        G.FUNCS.text_input_key({ key = "return" })
        G.CONTROLLER.text_input_hook = nil
      end
      FB.screen = name
      local slot = G.OVERLAY_MENU and G.OVERLAY_MENU:get_UIE_by_ID("bs_fb_body")
      if slot then
        slot.config.object:remove()
        slot.config.object = UIBox({
          definition = body(name),
          config = { offset = { x = 0, y = 0 }, parent = slot, type = "cm" },
        })
        slot.UIBox:recalculate()
      else
        G.FUNCS.overlay_menu({
          definition = create_UIBox_generic_options({
            back_func = "bs_fb_back",
            contents = {
              { n = G.UIT.O, config = { id = "bs_fb_body", object = UIBox({ definition = body(name), config = { type = "cm" } }) } },
            },
          }),
        })
      end
      return true
    end,
  }))
end

local function text_row(text, scale, colour)
  return {
    n = G.UIT.R,
    config = { align = "cm", padding = 0.03 },
    nodes = { { n = G.UIT.T, config = { text = text, scale = scale or 0.4, colour = colour or G.C.UI.TEXT_LIGHT } } },
  }
end

-- The warning shown instead of the Add / Edit buttons when FB.needs_run().
local function needs_run_rows()
  return {
    text_row("This game uses Steamodded's object weights (e.g. Pokermon):", 0.35, G.C.ORANGE),
    text_row("filters can only be created or edited during a run.", 0.35, G.C.ORANGE),
    text_row("Start or continue a run, then open the filters again.", 0.3, G.C.UI.TEXT_INACTIVE),
  }
end

local function small_button(label, func, colour, minw)
  return UIBox_button({
    label = { label },
    button = func,
    colour = colour or G.C.RED,
    minw = minw or 1.3,
    minh = 0.5,
    scale = 0.35,
    col = true,
  })
end

local function button_row(...)
  return { n = G.UIT.R, config = { align = "cm", padding = 0.1 }, nodes = { ... } }
end

local function labels(list, fmt)
  local out = {}
  for i, v in ipairs(list) do
    out[i] = fmt and fmt(v) or tostring(v)
  end
  return out
end

local with_tooltip
local function editing()
  return Brainstorm.config.filters.list[FB.filter_index]
end

-- Gives a row a hover tooltip (the game shows config.tooltip of whatever
-- the pointer is over).
with_tooltip = function(node, title, lines)
  node.config = node.config or {}
  node.config.tooltip = { title = title, text = lines }
  return node
end

-- ---------------------------------------------------------------------------
-- The saved filters, as rows for the Filters part of the Settings tab
-- (UI/settings.lua). Compact, so all FB.MAX_FILTERS fit without paging.

function FB.list_rows()
  local fs = Brainstorm.config.filters
  local rows = {
    text_row("Auto-reroll stops on a seed that matches the active filter.", 0.3, G.C.UI.TEXT_INACTIVE),
  }
  if FB.needs_run() then
    rows[#rows + 1] = text_row("This game uses object weights (e.g. Pokermon): edit filters during a run.", 0.3, G.C.ORANGE)
  end
  if #fs.list == 0 then
    rows[#rows + 1] = text_row("No saved filters.", 0.35, G.C.UI.TEXT_INACTIVE)
  end
  local function button(label, func, colour)
    return UIBox_button({ label = { label }, button = func, colour = colour, minw = 1.3, minh = 0.4, scale = 0.32, col = true })
  end
  for i, f in ipairs(fs.list) do
    local active = fs.active == i
    rows[#rows + 1] = {
      n = G.UIT.R,
      config = { align = "cm", padding = 0.03, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
      nodes = {
        {
          n = G.UIT.C,
          config = { align = "cl", minw = 5.5, padding = 0.03 },
          nodes = {
            { n = G.UIT.T, config = { text = f.name, scale = 0.36, colour = active and G.C.GREEN or G.C.UI.TEXT_LIGHT } },
            { n = G.UIT.T, config = { text = "  (" .. #f.conditions .. ")", scale = 0.28, colour = G.C.UI.TEXT_INACTIVE } },
          },
        },
        active and button("Active", "bs_fb_unuse", G.C.GREEN) or button("Use", "bs_fb_use_" .. i, G.C.GREY),
        button("Edit", "bs_fb_editf_" .. i, G.C.BLUE),
        button(FB.confirm_delete == i and "Sure?" or "Delete", "bs_fb_delf_" .. i, G.C.RED),
      },
    }
  end
  if #fs.list < FB.MAX_FILTERS then
    rows[#rows + 1] = button_row(small_button("New filter", "bs_fb_newf", G.C.GREEN, 3))
  end
  return rows
end

-- Back to Settings, on the Brainstorm-Rerolled tab's filter list.
function FB.to_settings()
  Brainstorm.open_settings_tab = true
  Brainstorm.ST.sub = "filters"
  G.FUNCS.settings()
end

-- ---------------------------------------------------------------------------
-- Screen: one filter

screens.filter = function()
  local f = editing()
  -- No filter being edited (it was deleted, or this file was reloaded):
  -- show the list instead.
  if not f then
    return { text_row("This filter no longer exists. Press Back.", 0.4) }
  end
  local locked = FB.needs_run()
  local rows = {
    text_row("EDIT FILTER", 0.6),
    {
      n = G.UIT.R,
      config = { align = "cm", padding = 0.05 },
      nodes = {
        { n = G.UIT.T, config = { text = "Name  ", scale = 0.4, colour = G.C.UI.TEXT_LIGHT } },
        create_text_input({
          w = 6,
          max_length = 26,
          prompt_text = "Filter name",
          ref_table = f,
          ref_value = "name",
          extended_corpus = true,
          callback = function()
            Brainstorm.writeConfig()
          end,
        }),
      },
    },
    with_tooltip(
      create_toggle({
        label = "Assume 1 of each enhanced card",
        scale = 0.8,
        ref_table = f,
        ref_value = "assume_enhanced",
        callback = function()
          Brainstorm.writeConfig()
        end,
      }),
      "Assume 1 of each enhanced card",
      {
        "Lets you search for jokers that only appear",
        "with an enhanced card in the deck (Steel Joker,",
        "Stone Joker, Lucky Cat, Golden Ticket, Glass Joker).",
        "Rarely (about 1 in 55 common or uncommon shop",
        "jokers) a run without those cards differs.",
      }
    ),
    with_tooltip(
      create_toggle({
        label = "Assume Showman",
        scale = 0.8,
        ref_table = f,
        ref_value = "assume_showman",
        callback = function()
          Brainstorm.writeConfig()
        end,
      }),
      "Assume Showman",
      {
        "As if you hold Showman all run: shop jokers",
        "can repeat in one shop, and two Souls can give",
        "the same legendary (e.g. two Perkeos).",
        "Leave off unless you plan to get Showman.",
      }
    ),
    text_row("Hover over an option for what it does.", 0.28, G.C.UI.TEXT_INACTIVE),
  }
  if #f.conditions == 0 then
    rows[#rows + 1] = text_row("No conditions yet: every seed matches.", 0.35, G.C.UI.TEXT_INACTIVE)
  end
  -- Soul conditions on the same antes take the Souls one after another:
  -- the list says which Soul each one is when there is more than one.
  local souls_on = {}
  for _, c in ipairs(f.conditions) do
    if c.kind == "soul" then
      local on = c.ante .. "-" .. c.ante_to
      souls_on[on] = (souls_on[on] or 0) + 1
    end
  end
  local soul_seen = {}
  for i, c in ipairs(f.conditions) do
    local nth
    if c.kind == "soul" then
      local on = c.ante .. "-" .. c.ante_to
      soul_seen[on] = (soul_seen[on] or 0) + 1
      nth = souls_on[on] > 1 and soul_seen[on] or nil
    end
    local line = {
      {
        n = G.UIT.C,
        config = { align = "cl", minw = 8.5, padding = 0.05 },
        nodes = { { n = G.UIT.T, config = { text = FB.describe(c, nth), scale = 0.35, colour = G.C.UI.TEXT_LIGHT } } },
      },
    }
    if not locked then
      line[#line + 1] = small_button("Edit", "bs_fb_edit_" .. i, G.C.BLUE)
    end
    line[#line + 1] = small_button("Remove", "bs_fb_remove_" .. i, G.C.RED)
    rows[#rows + 1] = {
      n = G.UIT.R,
      config = { align = "cm", padding = 0.04, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
      nodes = line,
    }
  end
  -- One button per kind of condition: it opens that kind's picker at once.
  if locked then
    for _, r in ipairs(needs_run_rows()) do
      rows[#rows + 1] = r
    end
  elseif #f.conditions < FB.MAX_CONDITIONS then
    local buttons = { { n = G.UIT.T, config = { text = "Add:  ", scale = 0.4, colour = G.C.UI.TEXT_LIGHT } } }
    for _, kind in ipairs(FB.KINDS) do
      -- (A long name gets a wider button, so its text isn't shrunk.)
      local label = FB.ADD_LABELS[kind]
      buttons[#buttons + 1] = small_button(label, "bs_fb_add_" .. kind, G.C.GREEN, #label > 7 and 2.1 or 1.35)
    end
    rows[#rows + 1] = { n = G.UIT.R, config = { align = "cm", padding = 0.08 }, nodes = buttons }
  end
  return rows
end

-- ---------------------------------------------------------------------------
-- Screen: one condition. The item on the left (a picker: grid + search), the
-- options on the right (a panel of its own, id "bs_fb_side", rebuilt when an
-- option or the item changes, so the picker keeps its page and search).
--
-- Sizes: the whole window must fit the screen at the game's 16:9 size, so
-- the picker shows 3 rows of 6 slightly smaller cards, or 4 of 5 chips, while the options panel is
-- beside it.

-- A button that runs act() when clicked (G.FUNCS.bs_fb_click). Selected
-- buttons are red, like the game's own option buttons.
local function choice_button(label, act, selected, minw)
  return {
    n = G.UIT.C,
    config = {
      align = "cm",
      padding = 0.05,
      r = 0.08,
      minw = minw or 0.45,
      minh = 0.42,
      hover = true,
      shadow = true,
      colour = selected and G.C.RED or G.C.BLUE,
      button = "bs_fb_click",
      ref_table = { act = act },
    },
    nodes = { { n = G.UIT.T, config = { text = tostring(label), scale = 0.32, colour = G.C.UI.TEXT_LIGHT } } },
  }
end

-- Rows of buttons, one per value; the current value is selected.
local function value_rows(values, names, current, set, per_row, minw)
  local rows, line = {}, {}
  for i, v in ipairs(values) do
    line[#line + 1] = choice_button(names and names[i] or v, function()
      set(v)
    end, v == current, minw)
    line[#line + 1] = { n = G.UIT.C, config = { minw = 0.06 } }
    if #line >= 2 * per_row then
      rows[#rows + 1] = { n = G.UIT.R, config = { align = "cl", padding = 0.03 }, nodes = line }
      line = {}
    end
  end
  if #line > 0 then
    rows[#rows + 1] = { n = G.UIT.R, config = { align = "cl", padding = 0.03 }, nodes = line }
  end
  return rows
end

-- The picker: what is being chosen ("legendary" for a Soul condition).
local function pick_kind_of(d)
  return d.kind == "soul" and "legendary" or d.kind
end

-- The name of what the condition being edited has chosen.
local function chosen_label(d)
  if d.kind == "soul" then
    return d.legendary == "any" and "Any legendary" or label_of("legendary", d.legendary)
  end
  return label_of(d.kind, d.key)
end

-- The options panel.
local function side()
  local d = FB.draft
  local rows = {}
  local function title(text, tip_title, tip)
    local node = text_row(text, 0.34)
    node.config.align = "cl"
    if tip then
      with_tooltip(node, tip_title, tip)
    end
    rows[#rows + 1] = node
  end
  local function add(list)
    for _, r in ipairs(list) do
      rows[#rows + 1] = r
    end
  end
  local function set(field)
    return function(v)
      d[field] = v
      FB.refresh_side()
    end
  end

  rows[#rows + 1] = {
    n = G.UIT.R,
    config = { align = "cl", padding = 0.03 },
    nodes = {
      { n = G.UIT.T, config = { text = (d.kind == "soul" and "Legendary" or FB.ADD_LABELS[d.kind]) .. ": ", scale = 0.38, colour = G.C.UI.TEXT_LIGHT } },
      { n = G.UIT.T, config = { text = chosen_label(d), scale = 0.38, colour = G.C.GOLD } },
    },
  }

  -- "Any legendary" is a choice of the item, but has no card in the grid:
  -- its button is here (below the grid it would make the window too tall
  -- for a game with many legendaries, like Pokermon).
  if d.kind == "soul" then
    add(value_rows({ "any" }, { "Any legendary" }, d.legendary, function()
      G.FUNCS.bs_fb_pick_any()
    end, 1, 2.4))
  end

  -- Antes: click one, or two for a range (the condition then holds if it
  -- holds in any of them).
  title("Antes")
  local line = {}
  for _, a in ipairs(FB.ANTES) do
    line[#line + 1] = choice_button(a, function()
      if FB.range_from then
        d.ante, d.ante_to = math.min(FB.range_from, a), math.max(FB.range_from, a)
        FB.range_from = nil
      else
        d.ante, d.ante_to = a, a
        FB.range_from = a
      end
      FB.refresh_side()
    end, a >= d.ante and a <= d.ante_to, 0.45)
    line[#line + 1] = { n = G.UIT.C, config = { minw = 0.05 } }
  end
  rows[#rows + 1] = { n = G.UIT.R, config = { align = "cl", padding = 0.03 }, nodes = line }
  rows[#rows + 1] = text_row(FB.range_from and "Now click the last ante (or the same one)." or "Click one ante, or two for a range.", 0.27, G.C.UI.TEXT_INACTIVE)
  rows[#rows].config.align = "cl"

  local editions = FB.editions()
  local edition_names = labels(editions, edition_label)
  if d.kind == "tag" then
    title("Blind")
    add(value_rows(FB.BLINDS, FB.BLIND_LABELS, d.blind, set("blind"), 3, 1.4))
  elseif d.kind == "pack" then
    title("Within the first ... shop packs")
    add(value_rows(FB.PACK_WITHIN, nil, d.within, set("within"), 6))
  elseif d.kind == "joker" then
    title("Where", "Where", {
      "Shop: among the first shop cards of the ante",
      "(2 per shop, plus 2 per reroll).",
      "Buffoon pack: in the first Buffoon packs you open",
      "in the ante (the first shop of ante 1 always",
      "has one; the Buffoon Tag gives a Mega one).",
    })
    add(value_rows(FB.JOKER_FROM, FB.JOKER_FROM_LABELS, d.from, set("from"), 2, 1.9))
    if d.from == "buffoon" then
      title("Within the first ... Buffoon packs")
      add(value_rows(FB.BUFFOON_PACKS, nil, d.packs, set("packs"), 6))
      title("Pack size")
      add(value_rows(FB.SIZES.buffoon, FB.SIZE_LABELS.buffoon, d.size, set("size"), 1, 3.2))
    else
      title("Within the first ... shop cards")
      add(value_rows(FB.JOKER_WITHIN, nil, d.within, set("within"), 6))
    end
    title("Edition")
    add(value_rows(editions, edition_names, d.edition, set("edition"), 3, 1.35))
  elseif d.kind == "card" then
    local froms = FB.card_froms(d.key)
    title("Where", "Where", {
      "A pack: the first packs of this kind you open in this",
      "ante, wherever they come from (the shop, or a tag).",
      "The same packs a Soul condition on this ante looks at:",
      "with both, the card is in the pack with The Soul.",
      "Shop: among the first shop cards of the ante",
      "(2 per shop, plus 2 per reroll).",
    })
    add(value_rows(froms, labels(froms, function(f)
      return FB.CARD_FROM_LABELS[f]
    end), d.from, function(v)
      if v ~= d.from then
        d.from = v
        if FB.SIZES[v] then
          d.size = FB.SIZES[v][1] -- the sizes differ per kind of pack
        end
      end
      FB.refresh_side()
    end, 2, 2.2))
    if d.from == "shop" then
      title("Within the first ... shop cards")
      add(value_rows(FB.JOKER_WITHIN, nil, d.within, set("within"), 6))
    else
      title("Within the first ... " .. FB.CARD_PACK_NAMES[d.from] .. " packs")
      add(value_rows(FB.BUFFOON_PACKS, nil, d.packs, set("packs"), 6))
      title("Pack size")
      add(value_rows(FB.SIZES[d.from], FB.SIZE_LABELS[d.from], d.size, set("size"), 1, 3.2))
    end
  elseif d.kind == "soul" then
    title("Pack", "Which packs", {
      "The first packs of this kind you open in this ante,",
      "wherever they come from: the shop, or a tag",
      "(Charm Tag: a Mega Arcana pack; Ethereal Tag,",
      "from ante 2 on: a normal Spectral pack).",
      "Two Souls in one ante? Add a second Soul condition:",
      "it takes the next pack.",
    })
    add(value_rows(FB.SOUL_FROM, FB.SOUL_FROM_LABELS, d.from, function(v)
      if v ~= d.from then
        d.from, d.size = v, FB.SIZES[v][1] -- the sizes differ per kind
      end
      FB.refresh_side()
    end, 2, 1.6))
    title("Pack size")
    add(value_rows(FB.SIZES[d.from], FB.SIZE_LABELS[d.from], d.size, set("size"), 1, 3.2))
    title("Edition")
    add(value_rows(editions, edition_names, d.edition, set("edition"), 3, 1.35))
  end

  rows[#rows + 1] = { n = G.UIT.R, config = { minh = 0.15 } }
  rows[#rows + 1] = {
    n = G.UIT.R,
    config = { align = "cm", padding = 0.05 },
    nodes = {
      small_button("Save", "bs_fb_save", G.C.GREEN, 1.8),
      { n = G.UIT.C, config = { minw = 0.15 } },
      small_button("Cancel", "bs_fb_cancel", G.C.GREY, 1.8),
    },
  }
  return {
    n = G.UIT.ROOT,
    config = { align = "cm", colour = G.C.CLEAR },
    nodes = { { n = G.UIT.C, config = { align = "tl", padding = 0.12, r = 0.1, minw = 5.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.3) }, nodes = rows } },
  }
end

-- Rebuilds the options panel in place. One frame later: it is called from
-- buttons inside the panel.
function FB.refresh_side()
  G.E_MANAGER:add_event(Event({ blockable = false, blocking = false, timer = "REAL", func = function()
    local slot = G.OVERLAY_MENU and G.OVERLAY_MENU:get_UIE_by_ID("bs_fb_side")
    if slot then
      slot.config.object:remove()
      slot.config.object = UIBox({ definition = side(), config = { offset = { x = 0, y = 0 }, parent = slot, type = "cm" } })
      slot.UIBox:recalculate()
    end
    return true
  end }))
end

-- ---------------------------------------------------------------------------
-- The picker (left side of the editor). Every kind of item is chosen the way
-- the game's Collection shows it: jokers, vouchers and packs as cards, tags
-- and bosses as their chips, a page at a time. A search box narrows the list
-- by name.
--
-- The grid is a UIBox of its own (id "bs_fb_grid") that is rebuilt when the
-- search text or the page changes. Rebuilding only the grid keeps the
-- keyboard in the search box while typing.

FB.PICK_ROWS, FB.PICK_COLS = 3, 6 -- cards per page
FB.PICK_SCALE = 0.8 -- card size in the picker (1 = the Collection's)
FB.TILE_ROWS, FB.TILE_COLS = 4, 5 -- tags / bosses per page
-- Every tag / boss slot has this size, filled or not, so a page that isn't
-- full is exactly as big as one that is (a boss chip and its name need
-- about 1.48 high; a long name is shrunk to fit the width).
FB.TILE_W, FB.TILE_H = 1.9, 1.55

-- Kinds shown as cards; the rest are shown as tiles (chip + name).
local CARD_KINDS = { joker = true, legendary = true, voucher = true, pack = true, card = true }

-- The items that can be picked: no "Any" entry (that's a button), and only
-- those whose name or key contains the search text.
local function pickable(kind)
  local out = {}
  local want = string.lower(FB.search_applied or "")
  for _, it in ipairs(FB.items(kind)) do
    if it.key ~= "any" then
      if want == "" or string.find(string.lower(it.label), want, 1, true) or string.find(string.lower(it.key), want, 1, true) then
        out[#out + 1] = it
      end
    end
  end
  return out
end

-- What the condition being edited has chosen for the kind in the picker.
local function current_pick()
  if FB.pick_kind == "legendary" then
    return FB.draft.legendary
  end
  return FB.draft.key
end

-- The game object behind an item key (a pack stands for its artworks).
local function center_of(kind, key)
  if kind == "pack" then
    return G.P_CENTERS[key .. "_1"] or G.P_CENTERS[key]
  end
  return G.P_CENTERS[key]
end

local function short(text, n)
  if #text > n then
    return string.sub(text, 1, n - 2) .. ".."
  end
  return text
end

-- A tile: the tag's or boss's chip with its name under it. Both the chip
-- and the name can be clicked. Hovering the chip shows the game's own
-- description, as in the Collection.
local function tile(kind, it, chosen)
  local object, node
  if kind == "tag" then
    local tag = Tag(it.key, true)
    local ui, sprite = tag:generate_UI()
    object, node = sprite, ui
  else
    local blind = G.P_BLINDS[it.key]
    local sprite = AnimatedSprite(0, 0, 1, 1, G.ANIMATION_ATLAS[blind.atlas or "blind_chips"], blind.pos)
    sprite:define_draw_steps({ { shader = "dissolve", shadow_height = 0.05 }, { shader = "dissolve" } })
    sprite.float = true
    sprite.states.hover.can = true
    sprite.states.drag.can = false
    sprite.states.collide.can = true
    sprite.config = { blind = blind, force_focus = true }
    sprite.hover = function()
      if not sprite.hovering and sprite.states.visible then
        sprite.hovering = true
        sprite.hover_tilt = 3
        sprite:juice_up(0.05, 0.02)
        sprite.config.h_popup = create_UIBox_blind_popup(blind, true)
        sprite.config.h_popup_config = { align = "cl", offset = { x = -0.1, y = 0 }, parent = sprite }
        Node.hover(sprite)
      end
    end
    sprite.stop_hover = function()
      sprite.hovering = false
      Node.stop_hover(sprite)
      sprite.hover_tilt = 0
    end
    object, node = sprite, { n = G.UIT.O, config = { object = sprite } }
  end
  if object then
    object.click = function()
      G.FUNCS.bs_fb_picked(it.key)
    end
  end
  return {
    n = G.UIT.C,
    config = { align = "cm", padding = 0.06, minw = FB.TILE_W, maxw = FB.TILE_W, minh = FB.TILE_H, r = 0.1, colour = chosen and darken(G.C.GREEN, 0.4) or G.C.CLEAR },
    nodes = {
      { n = G.UIT.R, config = { align = "cm" }, nodes = { node } },
      {
        n = G.UIT.R,
        config = { align = "cm", minh = 0.3, maxw = FB.TILE_W - 0.12, button = "bs_fb_pick_tile", ref_table = { key = it.key }, hover = true },
        nodes = { { n = G.UIT.T, config = { text = short(it.label, 17), scale = 0.27, colour = G.C.UI.TEXT_LIGHT } } },
      },
    },
  }
end

-- The grid: one page of the (searched) list, and the page selector.
--
-- Its size depends only on the kind of item, never on the page or the
-- search, so nothing moves while paging or typing: a short page keeps its
-- empty rows and slots, and the page selector stays (as "Page 1/1" when a
-- search leaves one page). A kind whose whole list fits on one page (the 5
-- legendaries of the unmodded game) gets just the rows that list needs and
-- no page selector.
local function grid()
  local kind = FB.pick_kind
  local list = pickable(kind)
  local cards = CARD_KINDS[kind]
  local n_rows, n_cols = cards and FB.PICK_ROWS or FB.TILE_ROWS, cards and FB.PICK_COLS or FB.TILE_COLS
  local per_page = n_rows * n_cols
  local pages = math.max(1, math.ceil(#list / per_page))
  FB.pick_page = math.max(1, math.min(FB.pick_page or 1, pages))
  local first = (FB.pick_page - 1) * per_page
  local current = current_pick()
  local all = 0 -- the whole list, without the search
  for _, it in ipairs(FB.items(kind)) do
    if it.key ~= "any" then
      all = all + 1
    end
  end
  local paged = all > per_page
  n_rows = math.max(1, math.min(n_rows, math.ceil(all / n_cols)))

  local rows = {}
  for r = 1, n_rows do
    if cards then
      -- The same kind of card row the Collection screen uses.
      local cw, ch = G.CARD_W * FB.PICK_SCALE, G.CARD_H * FB.PICK_SCALE
      local area = CardArea(G.ROOM.T.x + 0.2 * G.ROOM.T.w / 2, G.ROOM.T.h, n_cols * cw, 0.95 * ch, {
        card_limit = n_cols,
        type = "title",
        highlight_limit = 0,
        collection = true,
      })
      for col = 1, n_cols do
        local it = list[first + (r - 1) * n_cols + col]
        local center = it and center_of(kind, it.key)
        if center then
          -- bypass_discovery_*: show the real card even if not yet
          -- discovered (as the shop does).
          local card = Card(area.T.x + area.T.w / 2, area.T.y, cw, ch, nil, center, {
            bypass_discovery_center = true,
            bypass_discovery_ui = true,
          })
          card.bs_pick = it.key -- see the Card:click hook below
          area:emplace(card)
        end
      end
      rows[r] = { n = G.UIT.R, config = { align = "cm", padding = 0.07, no_fill = true }, nodes = { { n = G.UIT.O, config = { object = area } } } }
    else
      local tiles = {}
      for col = 1, n_cols do
        local it = list[first + (r - 1) * n_cols + col]
        if it then
          tiles[#tiles + 1] = tile(kind, it, it.key == current)
        else
          -- An empty slot, the size of a tile.
          tiles[#tiles + 1] = { n = G.UIT.C, config = { minw = FB.TILE_W, minh = FB.TILE_H } }
        end
      end
      rows[r] = { n = G.UIT.R, config = { align = "cl" }, nodes = tiles }
    end
  end

  -- Nothing found: the message takes the first row's place (same height).
  if #list == 0 then
    local h = cards and (0.95 * G.CARD_H * FB.PICK_SCALE + 0.14) or FB.TILE_H
    -- (A card row is its cards plus 0.07 padding on each side.)
    local w = cards and (n_cols * G.CARD_W * FB.PICK_SCALE + 0.14) or n_cols * FB.TILE_W
    rows[1] = { n = G.UIT.R, config = { align = "cm", minh = h, minw = w }, nodes = {
      { n = G.UIT.T, config = { text = "Nothing matches '" .. FB.search_applied .. "'.", scale = 0.35, colour = G.C.UI.TEXT_INACTIVE } },
    } }
  end
  local nodes = { { n = G.UIT.R, config = { align = "cm", r = 0.1, colour = G.C.BLACK, emboss = 0.05, padding = cards and 0 or 0.1 }, nodes = rows } }
  if paged then
    local page_names = {}
    for i = 1, pages do
      page_names[i] = localize("k_page") .. " " .. i .. "/" .. pages
    end
    nodes[#nodes + 1] = {
      n = G.UIT.R,
      config = { align = "cm" },
      nodes = {
        create_option_cycle({
          options = page_names,
          w = 4.5,
          cycle_shoulders = true,
          opt_callback = "bs_fb_pick_page",
          current_option = FB.pick_page,
          colour = G.C.RED,
          no_pips = true,
        }),
      },
    }
  end
  return { n = G.UIT.ROOT, config = { align = "cm", colour = G.C.CLEAR }, nodes = nodes }
end

-- Rebuilds the grid in place (new search text or page).
function FB.refresh_grid()
  local slot = G.OVERLAY_MENU and G.OVERLAY_MENU:get_UIE_by_ID("bs_fb_grid")
  if not slot then
    return
  end
  slot.config.object:remove()
  slot.config.object = UIBox({ definition = grid(), config = { offset = { x = 0, y = 0 }, parent = slot, type = "cm" } })
  slot.UIBox:recalculate()
end

local function picker()
  local kind = FB.pick_kind
  local cards = CARD_KINDS[kind]
  local what = kind == "legendary" and "legendary joker" or string.lower(FB.ADD_LABELS[kind] or kind)
  local nodes = {
    text_row("Click a " .. what .. " to choose it", 0.36),
    {
      n = G.UIT.R,
      -- `func` runs every frame: it notices typing in the search box.
      config = { align = "cm", padding = 0.05, func = "bs_fb_search_watch" },
      nodes = {
        { n = G.UIT.T, config = { text = "Search  ", scale = 0.36, colour = G.C.UI.TEXT_LIGHT } },
        create_text_input({
          w = 3.5,
          max_length = 18,
          prompt_text = "type a name",
          ref_table = FB,
          ref_value = "search",
          extended_corpus = true,
        }),
        { n = G.UIT.C, config = { minw = 0.15 } },
        small_button("Clear", "bs_fb_search_clear", G.C.GREY, 1.1),
      },
    },
    -- The card rows are a little shorter than the cards (as in the game's
    -- Collection), so the top row reaches above its row: a gap keeps the
    -- search row clear of it.
    { n = G.UIT.R, config = { minh = cards and 0.25 or 0.05 } },
    { n = G.UIT.R, config = { align = "cm" }, nodes = { { n = G.UIT.O, config = { id = "bs_fb_grid", object = UIBox({ definition = grid(), config = { type = "cm" } }) } } } },
  }
  return nodes
end

screens.editor = function()
  local d = FB.draft
  local title = (FB.draft_index and "EDIT CONDITION: " or "NEW CONDITION: ") .. string.upper(FB.KIND_LABELS[d.kind] or d.kind)
  return {
    text_row(title, 0.5),
    {
      n = G.UIT.R,
      config = { align = "tm", padding = 0.05 },
      nodes = {
        { n = G.UIT.C, config = { align = "tm" }, nodes = picker() },
        { n = G.UIT.C, config = { minw = 0.2 } },
        { n = G.UIT.C, config = { align = "tm" }, nodes = { { n = G.UIT.O, config = { id = "bs_fb_side", object = UIBox({ definition = side(), config = { type = "cm" } }) } } } },
      },
    },
  }
end

-- Clicking a picker card chooses it; every other card behaves as before.
local card_click_ref = Card.click
function Card:click()
  if self.bs_pick then
    G.FUNCS.bs_fb_picked(self.bs_pick)
    return
  end
  return card_click_ref(self)
end

-- ---------------------------------------------------------------------------
-- Button and option-cycle handlers (Balatro calls G.FUNCS[name]). A button
-- only knows its function's name, so rows get one handler per index.

G.FUNCS.bs_fb_open = function()
  FB.ensure_config()
  FB.to_settings()
end

-- A choice_button.
G.FUNCS.bs_fb_click = function(e)
  local r = e.config.ref_table
  if r and r.act then
    r.act()
  end
end

-- The window's Back button: one step up.
G.FUNCS.bs_fb_back = function()
  if FB.screen == "editor" then
    FB.go("filter") -- the condition is not saved
  else
    FB.to_settings()
  end
end

-- The filter list (Settings > Brainstorm-Rerolled > Filters)
local function set_active(i)
  Brainstorm.config.filters.active = i
  Brainstorm.writeConfig()
end
G.FUNCS.bs_fb_unuse = function()
  set_active(0)
  Brainstorm.ST.show("filters")
end
G.FUNCS.bs_fb_newf = function()
  local fs = Brainstorm.config.filters
  fs.list[#fs.list + 1] = { name = "New filter", assume_enhanced = false, assume_showman = false, conditions = {} }
  FB.filter_index = #fs.list
  Brainstorm.writeConfig()
  FB.go("filter")
end
for i = 1, FB.MAX_FILTERS do
  G.FUNCS["bs_fb_use_" .. i] = function()
    FB.confirm_delete = nil
    set_active(i)
    Brainstorm.ST.show("filters")
  end
  G.FUNCS["bs_fb_editf_" .. i] = function()
    FB.confirm_delete = nil
    FB.filter_index = i
    FB.go("filter")
  end
  -- Delete needs a second click ("Sure?").
  G.FUNCS["bs_fb_delf_" .. i] = function()
    if FB.confirm_delete ~= i then
      FB.confirm_delete = i
    else
      FB.confirm_delete = nil
      local fs = Brainstorm.config.filters
      table.remove(fs.list, i)
      if fs.active == i then
        fs.active = 0
      elseif fs.active > i then
        fs.active = fs.active - 1
      end
      Brainstorm.writeConfig()
    end
    Brainstorm.ST.show("filters")
  end
end

-- Opens the editor on a condition (a copy; index nil for a new one).
local function open_editor(draft, index)
  if FB.needs_run() then
    FB.go("filter") -- shows why
    return
  end
  FB.draft, FB.draft_index = draft, index
  FB.pick_kind = pick_kind_of(draft)
  FB.search, FB.search_applied = "", ""
  FB.range_from = nil
  -- Start on the page that shows the current choice.
  FB.pick_page = 1
  if index then
    local per_page = CARD_KINDS[FB.pick_kind] and FB.PICK_ROWS * FB.PICK_COLS or FB.TILE_ROWS * FB.TILE_COLS
    for i, it in ipairs(pickable(FB.pick_kind)) do
      if it.key == current_pick() then
        FB.pick_page = math.ceil(i / per_page)
      end
    end
  end
  FB.go("editor")
end

-- Filter screen: "Add: Tag | Voucher | ..." opens the editor on a new
-- condition of that kind.
for _, kind in ipairs(FB.KINDS) do
  G.FUNCS["bs_fb_add_" .. kind] = function()
    open_editor(FB.new_condition(kind), nil)
  end
end
for i = 1, FB.MAX_CONDITIONS do
  G.FUNCS["bs_fb_edit_" .. i] = function()
    local c = editing().conditions[i]
    if c then
      open_editor(copy_table(c), i)
    end
  end
  G.FUNCS["bs_fb_remove_" .. i] = function()
    table.remove(editing().conditions, i)
    Brainstorm.writeConfig()
    FB.go("filter")
  end
end

-- Editor
G.FUNCS.bs_fb_cancel = function()
  FB.go("filter")
end

G.FUNCS.bs_fb_save = function()
  local f = editing()
  local d = FB.draft
  FB.range_from = nil
  -- A joker that needs an enhanced card can only be found with the
  -- assumption on: switch it on rather than refuse.
  if d.kind == "joker" and FB.is_gated(d.key) and not f.assume_enhanced then
    f.assume_enhanced = true
    saveManagerAlert("Turned on 'Assume 1 of each enhanced card' (needed for this joker)")
  end
  -- The same legendary from two Souls needs a Showman: likewise.
  if d.kind == "soul" and d.legendary ~= "any" and not f.assume_showman then
    for i, c in ipairs(f.conditions) do
      if i ~= FB.draft_index and c.kind == "soul" and c.legendary == d.legendary then
        f.assume_showman = true
        saveManagerAlert("Turned on 'Assume Showman' (the same legendary twice needs it)")
        break
      end
    end
  end
  -- Ask the native library whether the filter with this change is valid
  -- (e.g. two Soul conditions on one ante must use the same pack size).
  local trial = {
    assume_enhanced = f.assume_enhanced,
    assume_showman = f.assume_showman,
    conditions = copy_table(f.conditions),
  }
  trial.conditions[FB.draft_index or (#trial.conditions + 1)] = d
  local problem = FB.problem(trial)
  if problem then
    saveManagerAlert("Can't save: " .. problem)
    return
  end
  f.conditions[FB.draft_index or (#f.conditions + 1)] = d
  Brainstorm.writeConfig()
  FB.go("filter")
end

-- Picker
G.FUNCS.bs_fb_pick_page = function(args)
  if args and args.cycle_config then
    FB.pick_page = args.cycle_config.current_option
    -- One frame later: this is called from the page selector inside the grid.
    G.E_MANAGER:add_event(Event({ blockable = false, blocking = false, timer = "REAL", func = function()
      FB.refresh_grid()
      return true
    end }))
  end
end
-- Runs every frame while the editor is open (see picker()).
G.FUNCS.bs_fb_search_watch = function()
  if FB.screen == "editor" and FB.search ~= FB.search_applied then
    FB.search_applied = FB.search
    FB.pick_page = 1
    FB.refresh_grid()
  end
end
G.FUNCS.bs_fb_search_clear = function()
  if FB.search ~= "" then
    FB.search, FB.search_applied = "", ""
    FB.go("editor") -- rebuilds the search box too
  end
end
-- An item was clicked: it becomes the condition's choice. The grid stays;
-- the options panel shows the new choice, and a chip grid moves its
-- highlight.
G.FUNCS.bs_fb_picked = function(key)
  if FB.pick_kind == "legendary" then
    FB.draft.legendary = key
  else
    FB.draft.key = key
  end
  -- A card that can't come from where the condition looks (a Planet in an
  -- Arcana pack): look in its own kind of pack instead.
  local d = FB.draft
  if d.kind == "card" then
    local froms = FB.card_froms(key)
    if d.from ~= froms[1] and d.from ~= froms[2] then
      d.from = froms[1]
      d.size = FB.SIZES[d.from] and FB.SIZES[d.from][1] or d.size
    end
  end
  FB.refresh_side()
  if not CARD_KINDS[FB.pick_kind] then
    G.E_MANAGER:add_event(Event({ blockable = false, blocking = false, timer = "REAL", func = function()
      FB.refresh_grid()
      return true
    end }))
  end
end
-- A tile's name was clicked (the chip calls bs_fb_picked itself).
G.FUNCS.bs_fb_pick_tile = function(e)
  G.FUNCS.bs_fb_picked(e.config.ref_table.key)
end
G.FUNCS.bs_fb_pick_any = function()
  G.FUNCS.bs_fb_picked("any")
end

return FB
