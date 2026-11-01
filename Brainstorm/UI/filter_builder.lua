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
    filter   one filter: its name, the "assume enhanced cards" option, and
             its conditions (add, edit, remove).
    editor   one condition: kind, item, antes, and that kind's options.
    picker   a grid of joker cards like the Collection screen, to choose a
             joker (or the Soul's legendary) by clicking it.

  All item lists come from the game's data (G.P_CENTER_POOLS) and all names
  from localize(), so they are in the player's language.
]]

local FB = {}
Brainstorm.FB = FB

FB.MAX_FILTERS = 10
FB.MAX_CONDITIONS = 12

-- ---------------------------------------------------------------------------
-- What can be chosen

FB.KINDS = { "tag", "voucher", "pack", "joker", "soul", "boss" }
FB.KIND_LABELS = {
  tag = "Tag",
  voucher = "Voucher",
  pack = "Booster pack",
  joker = "Joker in shop",
  soul = "Soul in Arcana pack",
  boss = "Boss blind",
}

FB.ANTES = { 1, 2, 3, 4, 5, 6, 7, 8 }
FB.BLINDS = { "any", "small", "big" }
FB.BLIND_LABELS = { "Either blind", "Small Blind", "Big Blind" }
FB.PACK_WITHIN = { 1, 2, 3, 4, 6 }
FB.JOKER_WITHIN = { 1, 2, 4, 6, 8, 10, 15, 20, 30, 50, 100, 150 }
FB.EDITIONS = { "any", "e_base", "e_foil", "e_holo", "e_polychrome", "e_negative" }
FB.SOUL_PACKS = { 1, 2, 3 }
FB.SIZES = { 5, 3 }
FB.SIZE_LABELS = { "Mega / Jumbo (5 cards)", "Normal (3 cards)" }

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
    c.key, c.within, c.edition = "j_blueprint", 4, "any"
  elseif kind == "soul" then
    c.packs, c.size, c.legendary = 1, 5, "any"
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
  if c.kind == "tag" then
    return string.format("tag %s ante=%s blind=%s", c.key, ante, c.blind)
  elseif c.kind == "voucher" then
    return string.format("voucher %s ante=%s", c.key, ante)
  elseif c.kind == "pack" then
    return string.format("pack %s ante=%s within=%d", c.key, ante, c.within)
  elseif c.kind == "joker" then
    return string.format("joker %s ante=%s within=%d edition=%s", c.key, ante, c.within, c.edition)
  elseif c.kind == "soul" then
    return string.format("soul ante=%s packs=%d size=%d legendary=%s", ante, c.packs, c.size, c.legendary)
  elseif c.kind == "boss" then
    return string.format("boss %s ante=%s", c.key, ante)
  end
  return ""
end

-- A short description for the condition list.
function FB.describe(c)
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
    return string.format("%s: %s%s in first %d shop cards", ante, label_of("joker", c.key), ed, c.within)
  elseif c.kind == "soul" then
    local leg = c.legendary == "any" and "The Soul" or ("Soul -> " .. label_of("legendary", c.legendary))
    local packs = c.packs == 1 and "an Arcana pack" or (c.packs .. " Arcana packs in a row")
    return string.format("%s: %s in %s", ante, leg, packs)
  elseif c.kind == "boss" then
    return string.format("%s: boss %s", ante, label_of("boss", c.key))
  end
  return "?"
end

-- A filter as native text.
function FB.filter_spec(f)
  local lines = {}
  if f.assume_enhanced then
    lines[1] = "assume enhanced"
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
function FB.problem(f)
  if not (Brainstorm.native and Brainstorm.native.lib) then
    return nil
  end
  local ok, err = Brainstorm.native.check_seed({ filter_spec = FB.filter_spec(f) }, "1")
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
  c.ante_to = c.ante_to or c.ante
  return c
end

-- The filters a new install starts with. (Each condition here matches the
-- old built-in menu option of the same name; Immolate's filterprog_test
-- checks they give the old answers.)
function FB.starter_filters()
  return {
    { name = "Charm + Soul", conditions = {
      cond({ kind = "tag", key = "tag_charm", blind = "small" }),
      cond({ kind = "soul" }),
    } },
    { name = "Charm + Soul + Observatory", conditions = {
      cond({ kind = "tag", key = "tag_charm", blind = "small" }),
      cond({ kind = "soul" }),
      cond({ kind = "voucher", key = "v_telescope" }),
      cond({ kind = "voucher", key = "v_observatory", ante = 2 }),
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
  local souls = tonumber(f.soul_skip) or 0
  if souls > 0 then
    list[#list + 1] = cond({ kind = "soul", packs = souls, legendary = f.inst_perkeo and "j_perkeo" or "any" })
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
  return { name = "Old settings", conditions = list }
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
    f.conditions = type(f.conditions) == "table" and f.conditions or {}
    for _, c in ipairs(f.conditions) do
      c.ante_to = c.ante_to or c.ante
    end
  end
  if type(fs.active) ~= "number" or not fs.list[fs.active] then
    fs.active = 0
  end
end

-- ---------------------------------------------------------------------------
-- The window and moving between screens

FB.screen = nil -- "filters", "filter", "editor" or "picker"
FB.filter_index = nil -- the filter being edited
FB.draft = nil -- the condition being edited (a copy; saved on "Save")
FB.draft_index = nil -- its position in the filter, or nil for a new one
FB.pick_kind = nil -- picker: "joker" or "legendary"
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

local function index_of(list, value, field)
  for i, v in ipairs(list) do
    if (field and v[field] or v) == value then
      return i
    end
  end
  return 1
end

local function labels(list, fmt)
  local out = {}
  for i, v in ipairs(list) do
    out[i] = fmt and fmt(v) or tostring(v)
  end
  return out
end

local function cycle(label, options, current, callback, w)
  return create_option_cycle({
    label = label,
    scale = 0.8,
    w = w or 5,
    options = options,
    current_option = current,
    opt_callback = callback,
    no_pips = #options > 12,
  })
end

local function editing()
  return Brainstorm.config.filters.list[FB.filter_index]
end

-- ---------------------------------------------------------------------------
-- Screen: the saved filters

screens.filters = function()
  local fs = Brainstorm.config.filters
  local rows = {
    text_row("FILTERS", 0.6),
    text_row("Auto-reroll (Ctrl+A) stops on a seed that matches the active filter.", 0.3, G.C.UI.TEXT_INACTIVE),
  }
  if #fs.list == 0 then
    rows[#rows + 1] = text_row("No saved filters.", 0.35, G.C.UI.TEXT_INACTIVE)
  end
  for i, f in ipairs(fs.list) do
    local active = fs.active == i
    rows[#rows + 1] = {
      n = G.UIT.R,
      config = { align = "cm", padding = 0.04, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
      nodes = {
        {
          n = G.UIT.C,
          config = { align = "cl", minw = 5.5, padding = 0.05 },
          nodes = {
            { n = G.UIT.T, config = { text = f.name, scale = 0.4, colour = active and G.C.GREEN or G.C.UI.TEXT_LIGHT } },
            { n = G.UIT.T, config = { text = "  (" .. #f.conditions .. ")", scale = 0.3, colour = G.C.UI.TEXT_INACTIVE } },
          },
        },
        active and small_button("Active", "bs_fb_unuse", G.C.GREEN) or small_button("Use", "bs_fb_use_" .. i, G.C.GREY),
        small_button("Edit", "bs_fb_editf_" .. i, G.C.BLUE),
        small_button(FB.confirm_delete == i and "Sure?" or "Delete", "bs_fb_delf_" .. i, G.C.RED),
      },
    }
  end
  if #fs.list < FB.MAX_FILTERS then
    rows[#rows + 1] = button_row(small_button("New filter", "bs_fb_newf", G.C.GREEN, 3))
  end
  return rows
end

-- ---------------------------------------------------------------------------
-- Screen: one filter

screens.filter = function()
  local f = editing()
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
    create_toggle({
      label = "Assume 1 of each enhanced card",
      scale = 0.8,
      ref_table = f,
      ref_value = "assume_enhanced",
      callback = function()
        Brainstorm.writeConfig()
      end,
    }),
    text_row("Lets you search for Steel Joker, Stone Joker, Lucky Cat, Golden Ticket and Glass Joker.", 0.3, G.C.UI.TEXT_INACTIVE),
    text_row("Rarely (about 1 in 55 common/uncommon shop jokers) a run without those cards differs.", 0.3, G.C.UI.TEXT_INACTIVE),
  }
  if #f.conditions == 0 then
    rows[#rows + 1] = text_row("No conditions yet: every seed matches.", 0.35, G.C.UI.TEXT_INACTIVE)
  end
  for i, c in ipairs(f.conditions) do
    rows[#rows + 1] = {
      n = G.UIT.R,
      config = { align = "cm", padding = 0.04, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
      nodes = {
        {
          n = G.UIT.C,
          config = { align = "cl", minw = 8.5, padding = 0.05 },
          nodes = { { n = G.UIT.T, config = { text = FB.describe(c), scale = 0.35, colour = G.C.UI.TEXT_LIGHT } } },
        },
        small_button("Edit", "bs_fb_edit_" .. i, G.C.BLUE),
        small_button("Remove", "bs_fb_remove_" .. i, G.C.RED),
      },
    }
  end
  if #f.conditions < FB.MAX_CONDITIONS then
    rows[#rows + 1] = button_row(small_button("Add condition", "bs_fb_add", G.C.GREEN, 3))
  end
  return rows
end

-- ---------------------------------------------------------------------------
-- Screen: one condition

-- "Joker: Blueprint  [Choose...]" row of the editor.
local function pick_button(label, current, kind)
  return {
    n = G.UIT.R,
    config = { align = "cm", padding = 0.05 },
    nodes = {
      {
        n = G.UIT.C,
        config = { align = "cm", minw = 4.5 },
        nodes = { { n = G.UIT.T, config = { text = label .. ": " .. current, scale = 0.4, colour = G.C.UI.TEXT_LIGHT } } },
      },
      small_button("Choose...", "bs_fb_pick_" .. kind, G.C.BLUE, 2),
    },
  }
end

screens.editor = function()
  local d = FB.draft
  local nodes = {
    text_row(FB.draft_index and "EDIT CONDITION" or "NEW CONDITION", 0.6),
    cycle("Kind", labels(FB.KINDS, function(k)
      return FB.KIND_LABELS[k]
    end), index_of(FB.KINDS, d.kind), "bs_fb_kind"),
  }
  if d.kind == "joker" then
    nodes[#nodes + 1] = pick_button("Joker", label_of("joker", d.key), "joker")
  elseif d.kind ~= "soul" then
    local items = FB.items(d.kind)
    nodes[#nodes + 1] = cycle("Item", labels(items, function(it)
      return it.label
    end), index_of(items, d.key, "key"), "bs_fb_item", 6)
  end
  -- Antes: one, or a range (the condition then holds if it holds in any
  -- of them).
  nodes[#nodes + 1] = {
    n = G.UIT.R,
    config = { align = "cm" },
    nodes = {
      { n = G.UIT.C, config = { align = "cm" }, nodes = { cycle("From ante", labels(FB.ANTES), index_of(FB.ANTES, d.ante), "bs_fb_ante", 2) } },
      { n = G.UIT.C, config = { align = "cm" }, nodes = { cycle("To ante", labels(FB.ANTES), index_of(FB.ANTES, d.ante_to), "bs_fb_ante_to", 2) } },
    },
  }
  if d.kind == "tag" then
    nodes[#nodes + 1] = cycle("Blind", FB.BLIND_LABELS, index_of(FB.BLINDS, d.blind), "bs_fb_blind")
  elseif d.kind == "pack" then
    nodes[#nodes + 1] = cycle("Within the first ... packs", labels(FB.PACK_WITHIN), index_of(FB.PACK_WITHIN, d.within), "bs_fb_within")
  elseif d.kind == "joker" then
    nodes[#nodes + 1] = cycle("Within the first ... shop cards", labels(FB.JOKER_WITHIN), index_of(FB.JOKER_WITHIN, d.within), "bs_fb_within")
    nodes[#nodes + 1] = cycle("Edition", labels(FB.EDITIONS, edition_label), index_of(FB.EDITIONS, d.edition), "bs_fb_edition")
  elseif d.kind == "soul" then
    nodes[#nodes + 1] = pick_button("Legendary", label_of("legendary", d.legendary), "legendary")
    nodes[#nodes + 1] = cycle("Arcana packs in a row", labels(FB.SOUL_PACKS), index_of(FB.SOUL_PACKS, d.packs), "bs_fb_packs")
    nodes[#nodes + 1] = cycle("Pack size", FB.SIZE_LABELS, index_of(FB.SIZES, d.size), "bs_fb_size")
  end
  nodes[#nodes + 1] = button_row(small_button("Save", "bs_fb_save", G.C.GREEN, 2.5))
  return nodes
end

-- ---------------------------------------------------------------------------
-- Screen: picker (a grid of joker cards, like Collection > Jokers)

FB.PICK_ROWS, FB.PICK_COLS = 3, 5
FB.pick_areas = nil -- one CardArea per row

-- The cards that can be picked (no "Any" entry: that's a button).
local function pickable(kind)
  local out = {}
  for _, it in ipairs(FB.items(kind)) do
    if it.key ~= "any" then
      out[#out + 1] = it
    end
  end
  return out
end

-- Puts page `page` of the list into the card rows, replacing what's there.
-- Same steps as the game's G.FUNCS.your_collection_joker_page.
function FB.fill_picker(page)
  local list = pickable(FB.pick_kind)
  local per_page = FB.PICK_ROWS * FB.PICK_COLS
  for _, area in ipairs(FB.pick_areas) do
    for i = #area.cards, 1, -1 do
      local c = area:remove_card(area.cards[i])
      c:remove()
    end
  end
  for r = 1, FB.PICK_ROWS do
    local area = FB.pick_areas[r]
    for col = 1, FB.PICK_COLS do
      local it = list[(page - 1) * per_page + (r - 1) * FB.PICK_COLS + col]
      if it then
        -- bypass_discovery_*: show the real card even if not yet discovered
        -- (as the shop does).
        local card = Card(area.T.x + area.T.w / 2, area.T.y, G.CARD_W, G.CARD_H, nil, G.P_CENTERS[it.key], {
          bypass_discovery_center = true,
          bypass_discovery_ui = true,
        })
        card.bs_pick = it.key -- see the Card:click hook below
        area:emplace(card)
      end
    end
  end
end

screens.picker = function()
  local kind = FB.pick_kind
  local list = pickable(kind)
  local per_page = FB.PICK_ROWS * FB.PICK_COLS
  local pages = math.max(1, math.ceil(#list / per_page))
  local current = kind == "joker" and FB.draft.key or FB.draft.legendary

  -- Start on the page that shows the current choice.
  local page = 1
  for i, it in ipairs(list) do
    if it.key == current then
      page = math.ceil(i / per_page)
    end
  end

  local rows = {}
  FB.pick_areas = {}
  for r = 1, FB.PICK_ROWS do
    -- The same kind of card row the Collection screen uses.
    FB.pick_areas[r] = CardArea(G.ROOM.T.x + 0.2 * G.ROOM.T.w / 2, G.ROOM.T.h, FB.PICK_COLS * G.CARD_W, 0.95 * G.CARD_H, {
      card_limit = FB.PICK_COLS,
      type = "title",
      highlight_limit = 0,
      collection = true,
    })
    rows[r] = { n = G.UIT.R, config = { align = "cm", padding = 0.07, no_fill = true }, nodes = { { n = G.UIT.O, config = { object = FB.pick_areas[r] } } } }
  end
  FB.fill_picker(page)

  local page_names = {}
  for i = 1, pages do
    page_names[i] = localize("k_page") .. " " .. i .. "/" .. pages
  end
  -- The card rows are a little shorter than the cards (as in the game's
  -- Collection), so the top row reaches above its row: the hint gets extra
  -- padding to stay clear of it.
  local hint = text_row("Click a joker to choose it (now: " .. label_of(kind, current) .. ")", 0.4)
  hint.config.padding = 0.15
  local contents = {
    hint,
    { n = G.UIT.R, config = { align = "cm", r = 0.1, colour = G.C.BLACK, emboss = 0.05 }, nodes = rows },
  }
  if pages > 1 then
    contents[#contents + 1] = {
      n = G.UIT.R,
      config = { align = "cm" },
      nodes = {
        create_option_cycle({
          options = page_names,
          w = 4.5,
          cycle_shoulders = true,
          opt_callback = "bs_fb_pick_page",
          current_option = page,
          colour = G.C.RED,
          no_pips = true,
        }),
      },
    }
  end
  if kind == "legendary" then
    contents[#contents + 1] = button_row(small_button("Any legendary", "bs_fb_pick_any", G.C.GREEN, 3))
  end
  return contents
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
  FB.go("filters")
end

-- The window's Back button: one step up.
G.FUNCS.bs_fb_back = function()
  if FB.screen == "picker" then
    FB.go("editor")
  elseif FB.screen == "editor" then
    FB.go("filter")
  elseif FB.screen == "filter" then
    FB.go("filters")
  else
    -- Back to Settings, on the Brainstorm tab (see UI/ui.lua).
    Brainstorm.open_settings_tab = true
    G.FUNCS.settings()
  end
end

-- Filters screen
local function set_active(i)
  Brainstorm.config.filters.active = i
  Brainstorm.writeConfig()
end
G.FUNCS.bs_fb_unuse = function()
  set_active(0)
  FB.go("filters")
end
G.FUNCS.bs_fb_newf = function()
  local fs = Brainstorm.config.filters
  fs.list[#fs.list + 1] = { name = "New filter", assume_enhanced = false, conditions = {} }
  FB.filter_index = #fs.list
  Brainstorm.writeConfig()
  FB.go("filter")
end
for i = 1, FB.MAX_FILTERS do
  G.FUNCS["bs_fb_use_" .. i] = function()
    FB.confirm_delete = nil
    set_active(i)
    FB.go("filters")
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
    FB.go("filters")
  end
end

-- Filter screen
G.FUNCS.bs_fb_add = function()
  FB.draft, FB.draft_index = FB.new_condition("joker"), nil
  FB.go("editor")
end
for i = 1, FB.MAX_CONDITIONS do
  G.FUNCS["bs_fb_edit_" .. i] = function()
    local c = editing().conditions[i]
    if c then
      FB.draft, FB.draft_index = copy_table(c), i
      FB.go("editor")
    end
  end
  G.FUNCS["bs_fb_remove_" .. i] = function()
    table.remove(editing().conditions, i)
    Brainstorm.writeConfig()
    FB.go("filter")
  end
end

-- Editor
G.FUNCS.bs_fb_kind = function(x)
  local kind = FB.KINDS[x.to_key]
  if kind ~= FB.draft.kind then
    local ante, ante_to = FB.draft.ante, FB.draft.ante_to
    FB.draft = FB.new_condition(kind)
    FB.draft.ante, FB.draft.ante_to = ante, ante_to
    FB.go("editor") -- each kind has different options
  end
end
G.FUNCS.bs_fb_item = function(x)
  FB.draft.key = FB.items(FB.draft.kind)[x.to_key].key
end
G.FUNCS.bs_fb_ante = function(x)
  FB.draft.ante = FB.ANTES[x.to_key]
end
G.FUNCS.bs_fb_ante_to = function(x)
  FB.draft.ante_to = FB.ANTES[x.to_key]
end
G.FUNCS.bs_fb_blind = function(x)
  FB.draft.blind = FB.BLINDS[x.to_key]
end
G.FUNCS.bs_fb_within = function(x)
  local list = FB.draft.kind == "pack" and FB.PACK_WITHIN or FB.JOKER_WITHIN
  FB.draft.within = list[x.to_key]
end
G.FUNCS.bs_fb_edition = function(x)
  FB.draft.edition = FB.EDITIONS[x.to_key]
end
G.FUNCS.bs_fb_packs = function(x)
  FB.draft.packs = FB.SOUL_PACKS[x.to_key]
end
G.FUNCS.bs_fb_size = function(x)
  FB.draft.size = FB.SIZES[x.to_key]
end

G.FUNCS.bs_fb_save = function()
  local f = editing()
  local d = FB.draft
  -- "To ante" before "From ante" means just that one ante.
  if d.ante_to < d.ante then
    d.ante_to = d.ante
  end
  -- A joker that needs an enhanced card can only be found with the
  -- assumption on: switch it on rather than refuse.
  if d.kind == "joker" and FB.is_gated(d.key) and not f.assume_enhanced then
    f.assume_enhanced = true
    saveManagerAlert("Turned on 'Assume 1 of each enhanced card' (needed for this joker)")
  end
  -- Ask the native library whether the filter with this change is valid
  -- (e.g. two Soul conditions on one ante must use the same pack size).
  local trial = { assume_enhanced = f.assume_enhanced, conditions = copy_table(f.conditions) }
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
G.FUNCS.bs_fb_pick_joker = function()
  FB.pick_kind = "joker"
  FB.go("picker")
end
G.FUNCS.bs_fb_pick_legendary = function()
  FB.pick_kind = "legendary"
  FB.go("picker")
end
G.FUNCS.bs_fb_pick_page = function(args)
  if args and args.cycle_config then
    FB.fill_picker(args.cycle_config.current_option)
  end
end
G.FUNCS.bs_fb_picked = function(key)
  if FB.pick_kind == "joker" then
    FB.draft.key = key
  else
    FB.draft.legendary = key
  end
  FB.go("editor")
end
G.FUNCS.bs_fb_pick_any = function()
  G.FUNCS.bs_fb_picked("any")
end

-- Settings > Brainstorm: the "Active filter" cycler.
G.FUNCS.bs_fb_active = function(x)
  set_active(x.to_key - 1) -- option 1 is "None"
end

-- The Settings > Brainstorm tab contents.
function FB.settings_tab()
  FB.ensure_config()
  local fs = Brainstorm.config.filters
  local names = { "None" }
  for i, f in ipairs(fs.list) do
    names[i + 1] = f.name
  end
  return {
    n = G.UIT.ROOT,
    config = { align = "cm", padding = 0.05, colour = G.C.CLEAR },
    nodes = {
      {
        n = G.UIT.C,
        config = { align = "cm", padding = 0.2, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.25) },
        nodes = {
          create_option_cycle({
            label = "ACTIVE FILTER",
            scale = 0.8,
            w = 7, -- wide enough for a 26-character filter name
            options = names,
            current_option = fs.active + 1,
            opt_callback = "bs_fb_active",
          }),
          text_row("Used by auto-reroll (Ctrl+A).", 0.3, G.C.UI.TEXT_INACTIVE),
          {
            n = G.UIT.R,
            config = { align = "cm", padding = 0.1 },
            nodes = {
              UIBox_button({ label = { "EDIT FILTERS" }, button = "bs_fb_open", colour = G.C.BLUE, minw = 4, minh = 0.7, scale = 0.45 }),
            },
          },
        },
      },
    },
  }
end

return FB
