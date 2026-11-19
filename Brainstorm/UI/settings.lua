-- settings.lua — the Brainstorm-Rerolled tab of the game's Settings menu.
--
-- The tab has four parts, picked with the buttons along its top:
--   Filters      the saved filters (use, edit, delete, new). The rows come
--                from Brainstorm.FB.list_rows() (UI/filter_builder.lua);
--                "Edit" opens the filter's own window, whose Back button
--                comes back here.
--   Saved seeds  seeds the player kept (with a note), and a deck and stake
--                to play them with.
--   Hotkeys      which keys do what; click a key, then press the new one.
--   Mobile       the "Mobile Friendly" toggle and, when it is on, buttons
--                for everything the hotkeys do (UI/mobile.lua).
--
-- The tab is registered in UI/ui.lua. Switching parts replaces only the
-- contents of the "bs_st_body" slot, which always has the same size, so
-- the Settings window never moves or changes size.

local ST = {}
Brainstorm.ST = ST

ST.sub = "filters" -- the part shown: "filters", "seeds", "hotkeys" or "mobile"
ST.SUBS = {
  { "filters", "Filters" },
  { "seeds", "Saved seeds" },
  { "hotkeys", "Hotkeys" },
  { "mobile", "Mobile" },
}
ST.BODY_W, ST.BODY_H = 11, 6.1 -- the size of every part

ST.MAX_SEEDS = 100
ST.SEEDS_PER_PAGE = 5
ST.seed_page = 1
ST.confirm_delete = nil -- saved seed waiting for a second click on Delete
ST.confirm_play = nil -- saved seed waiting for a second click on Play (in a run)
ST.capturing = nil -- the hotkey waiting for a key press, or nil

-- Hotkeys: config key, label. "+" ones are pressed while holding the
-- modifier; the save state keys are held while pressing 1-5.
ST.DEFAULT_KEYS = {
  modifier = "lctrl",
  f_reroll = "r",
  a_reroll = "a",
  options = "t",
  save_seed = "s",
  s_state = "z",
  l_state = "x",
}
ST.HOTKEYS = {
  { "modifier", "Modifier (held with the + keys)" },
  { "f_reroll", "+ Reroll (a new random seed)" },
  { "a_reroll", "+ Auto-reroll (start / stop)" },
  { "options", "+ Open this menu" },
  { "save_seed", "+ Save this run's seed" },
  { "s_state", "Save state: hold, then press 1-5" },
  { "l_state", "Load state: hold, then press 1-5" },
}
local SAVE_SLOT_KEYS = { ["1"] = true, ["2"] = true, ["3"] = true, ["4"] = true, ["5"] = true }

-- Fills in what an older config.lua doesn't have yet.
function ST.ensure_config()
  local c = Brainstorm.config
  c.keybinds = c.keybinds or {}
  for k, v in pairs(ST.DEFAULT_KEYS) do
    if c.keybinds[k] == nil then
      c.keybinds[k] = v
    end
  end
  c.saved_seeds = c.saved_seeds or {}
  local ss = c.saved_seeds
  ss.list = ss.list or {}
  ss.deck = ss.deck or "b_red"
  ss.stake = ss.stake or 1
  for _, s in ipairs(ss.list) do
    s.note = s.note or ""
  end
end

-- ---------------------------------------------------------------------------
-- Small pieces

local KEY_NAMES = {
  lctrl = "Left Ctrl", rctrl = "Right Ctrl", lshift = "Left Shift", rshift = "Right Shift",
  lalt = "Left Alt", ralt = "Right Alt", lgui = "Left Cmd/Win", rgui = "Right Cmd/Win",
  space = "Space", tab = "Tab", ["return"] = "Enter", backspace = "Backspace",
}
-- "lctrl" -> "Left Ctrl", "a" -> "A".
function ST.key_name(k)
  if not k then
    return "-"
  end
  return KEY_NAMES[k] or (#k == 1 and k:upper()) or (k:gsub("^%l", string.upper))
end
-- How to press a hotkey, e.g. "Left Ctrl+S".
function ST.key_hint(action)
  local kb = Brainstorm.config.keybinds
  return ST.key_name(kb.modifier) .. "+" .. ST.key_name(kb[action])
end

local function text(str, scale, colour)
  return { n = G.UIT.T, config = { text = str, scale = scale or 0.35, colour = colour or G.C.UI.TEXT_LIGHT } }
end
local function text_row(str, scale, colour)
  return { n = G.UIT.R, config = { align = "cm", padding = 0.03 }, nodes = { text(str, scale, colour) } }
end
local function button(label, func, colour, minw)
  return UIBox_button({ label = { label }, button = func, colour = colour, minw = minw or 1.3, minh = 0.4, scale = 0.32, col = true })
end

-- Decks the player can pick (unlocked ones), and every stake, in order.
local function decks()
  local out = {}
  local all = G.PROFILES[G.SETTINGS.profile].all_unlocked
  for _, c in ipairs(G.P_CENTER_POOLS.Back or {}) do
    if all or c.unlocked ~= false then
      out[#out + 1] = c
    end
  end
  return out
end
local function stakes()
  local out = {}
  for _, c in ipairs(G.P_CENTER_POOLS.Stake or {}) do
    out[#out + 1] = c
  end
  table.sort(out, function(a, b)
    return (a.order or 0) < (b.order or 0)
  end)
  return out
end
local function name_of(set, c)
  local ok, name = pcall(localize, { type = "name_text", set = set, key = c.key })
  if ok and type(name) == "string" and name ~= "ERROR" then
    return name
  end
  return c.name or c.key
end

-- ---------------------------------------------------------------------------
-- The parts

local function seed_rows()
  local ss = Brainstorm.config.saved_seeds
  local in_run = G.STAGE == G.STAGES.RUN and G.GAME and G.GAME.pseudorandom and G.GAME.pseudorandom.seed
  local rows = {}

  rows[#rows + 1] = {
    n = G.UIT.R,
    config = { align = "cm", padding = 0.05, minh = 0.55 },
    nodes = {
      in_run and button("Save this run's seed", "bs_st_save_seed", G.C.GREEN, 3.6)
        or { n = G.UIT.C, config = { align = "cm", minw = 3.6 }, nodes = { text("Start a run to save its seed.", 0.32, G.C.UI.TEXT_INACTIVE) } },
      { n = G.UIT.C, config = { align = "cm", padding = 0.1 }, nodes = { text("(or " .. ST.key_hint("save_seed") .. " in a run)", 0.3, G.C.UI.TEXT_INACTIVE) } },
    },
  }

  -- The deck and stake Play uses.
  local deck_list, deck_names, deck_i = decks(), {}, 1
  for i, c in ipairs(deck_list) do
    deck_names[i] = name_of("Back", c)
    if c.key == ss.deck then
      deck_i = i
    end
  end
  local stake_list, stake_names = stakes(), {}
  for i, c in ipairs(stake_list) do
    stake_names[i] = name_of("Stake", c)
  end
  ST.deck_list = deck_list
  rows[#rows + 1] = {
    n = G.UIT.R,
    config = { align = "cm", padding = 0.02 },
    nodes = {
      { n = G.UIT.C, config = { align = "cm" }, nodes = {
        create_option_cycle({ label = "PLAY WITH DECK", scale = 0.6, w = 3.8, options = deck_names, current_option = deck_i, opt_callback = "bs_st_deck" }),
      } },
      { n = G.UIT.C, config = { align = "cm" }, nodes = {
        create_option_cycle({ label = "STAKE", scale = 0.6, w = 3.2, options = stake_names, current_option = math.min(ss.stake, #stake_names), opt_callback = "bs_st_stake" }),
      } },
    },
  }

  -- The list, a page at a time. Every slot has the same height, filled or
  -- not, and the page selector's place is kept, so nothing moves.
  local per = ST.SEEDS_PER_PAGE
  local pages = math.max(1, math.ceil(#ss.list / per))
  ST.seed_page = math.max(1, math.min(ST.seed_page, pages))
  local list = { n = G.UIT.C, config = { align = "tm", minh = per * 0.62 }, nodes = {} }
  if #ss.list == 0 then
    list.nodes[1] = text_row("No saved seeds yet.", 0.35, G.C.UI.TEXT_INACTIVE)
  end
  for slot = 1, per do
    local i = (ST.seed_page - 1) * per + slot
    local s = ss.list[i]
    if s then
      list.nodes[#list.nodes + 1] = {
        n = G.UIT.R,
        config = { align = "cm", padding = 0.04, r = 0.1, minh = 0.58, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
        nodes = {
          { n = G.UIT.C, config = { align = "cl", minw = 1.9, padding = 0.03 }, nodes = { text(s.seed, 0.38) } },
          -- Each note in a UIBox of its own: a UIBox draws one element per
          -- draw_layer, and a text input uses layers 1 and 2, so in one
          -- shared UIBox only the last note would be drawn.
          { n = G.UIT.O, config = { object = UIBox({
            definition = { n = G.UIT.ROOT, config = { align = "cm", colour = G.C.CLEAR }, nodes = {
              create_text_input({
                w = 4.6,
                max_length = 28,
                text_scale = 0.32,
                prompt_text = "Add a note",
                ref_table = s,
                ref_value = "note",
                extended_corpus = true,
                callback = function()
                  Brainstorm.writeConfig()
                end,
              }),
            } },
            config = { type = "cm" },
          }) } },
          button(ST.confirm_play == i and "Sure?" or "Play", "bs_st_play_" .. slot, G.C.GREEN, 1.1),
          button(ST.confirm_delete == i and "Sure?" or "Delete", "bs_st_del_" .. slot, G.C.RED, 1.1),
        },
      }
    end
  end
  rows[#rows + 1] = { n = G.UIT.R, config = { align = "cm" }, nodes = { list } }

  local pager = { n = G.UIT.R, config = { align = "cm", minh = 0.65 }, nodes = {} }
  if pages > 1 then
    local options = {}
    for p = 1, pages do
      options[p] = "Page " .. p .. "/" .. pages
    end
    pager.nodes[1] = create_option_cycle({ options = options, current_option = ST.seed_page, opt_callback = "bs_st_seed_page", w = 3, scale = 0.6, no_pips = true })
  end
  rows[#rows + 1] = pager
  return rows
end

local function hotkey_rows()
  local kb = Brainstorm.config.keybinds
  local rows = {
    text_row("Click a key, then press the new one (Esc cancels).", 0.3, G.C.UI.TEXT_INACTIVE),
  }
  for i, h in ipairs(ST.HOTKEYS) do
    local action, label = h[1], h[2]
    local waiting = ST.capturing == action
    rows[#rows + 1] = {
      n = G.UIT.R,
      config = { align = "cm", padding = 0.04, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
      nodes = {
        { n = G.UIT.C, config = { align = "cl", minw = 6, padding = 0.03 }, nodes = { text(label, 0.36) } },
        button(waiting and "Press a key..." or ST.key_name(kb[action]), "bs_st_key_" .. i, waiting and G.C.ORANGE or G.C.BLUE, 3),
      },
    }
  end
  rows[#rows + 1] = { n = G.UIT.R, config = { align = "cm", padding = 0.12 }, nodes = {
    button("Reset to defaults", "bs_st_keys_reset", G.C.RED, 3),
  } }
  return rows
end

-- The bar of part buttons, then the part, in a box of fixed size.
local function body()
  local bar = { n = G.UIT.R, config = { align = "cm", padding = 0.05 }, nodes = {} }
  for _, sub in ipairs(ST.SUBS) do
    bar.nodes[#bar.nodes + 1] = button(sub[2], "bs_st_sub_" .. sub[1], ST.sub == sub[1] and G.C.RED or G.C.GREY, 2.5)
  end
  local rows
  if ST.sub == "seeds" then
    rows = seed_rows()
  elseif ST.sub == "hotkeys" then
    rows = hotkey_rows()
  elseif ST.sub == "mobile" then
    rows = Brainstorm.MB.rows()
  else
    rows = Brainstorm.FB.list_rows()
  end
  return {
    n = G.UIT.ROOT,
    config = { align = "tm", colour = G.C.CLEAR },
    nodes = {
      bar,
      {
        n = G.UIT.R,
        config = { align = "tm", minw = ST.BODY_W, minh = ST.BODY_H, padding = 0.1, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.25) },
        nodes = { { n = G.UIT.C, config = { align = "tm", padding = 0.03 }, nodes = rows } },
      },
    },
  }
end

-- The tab's contents (called by create_tabs, UI/ui.lua).
function ST.tab()
  ST.ensure_config()
  Brainstorm.FB.ensure_config()
  ST.capturing = nil
  return {
    n = G.UIT.ROOT,
    config = { align = "cm", padding = 0.05, colour = G.C.CLEAR },
    nodes = {
      { n = G.UIT.R, config = { align = "cm" }, nodes = {
        { n = G.UIT.O, config = { id = "bs_st_body", object = UIBox({ definition = body(), config = { type = "cm" } }) } },
      } },
    },
  }
end

-- Shows a part (or the current one again, after a change). One frame
-- later, so it is safe to call from a button inside the part. If the
-- Settings window isn't open on this tab, it is opened.
function ST.show(sub)
  ST.sub = sub or ST.sub
  G.E_MANAGER:add_event(Event({
    blockable = false,
    blocking = false,
    timer = "REAL",
    func = function()
      -- Finish typing in a note that is about to disappear.
      if G.CONTROLLER.text_input_hook then
        G.FUNCS.text_input_key({ key = "return" })
        G.CONTROLLER.text_input_hook = nil
      end
      local slot = G.OVERLAY_MENU and G.OVERLAY_MENU:get_UIE_by_ID("bs_st_body")
      if slot then
        slot.config.object:remove()
        slot.config.object = UIBox({
          definition = body(),
          config = { offset = { x = 0, y = 0 }, parent = slot, type = "cm" },
        })
        slot.UIBox:recalculate()
      else
        Brainstorm.open_settings_tab = true
        G.FUNCS.settings(nil, true)
      end
      return true
    end,
  }))
end

-- Opens Settings on this tab (the "open menu" hotkey).
function ST.open()
  Brainstorm.open_settings_tab = true
  G.FUNCS.settings()
end

-- ---------------------------------------------------------------------------
-- Saved seeds

local function seeds_shown()
  return ST.sub == "seeds" and G.OVERLAY_MENU and G.OVERLAY_MENU:get_UIE_by_ID("bs_st_body")
end

-- Saves the current run's seed (newest first).
function ST.save_current_seed()
  if not (G.STAGE == G.STAGES.RUN and G.GAME and G.GAME.pseudorandom and G.GAME.pseudorandom.seed) then
    saveManagerAlert("Start a run to save its seed")
    return
  end
  ST.ensure_config()
  local seed = G.GAME.pseudorandom.seed
  local list = Brainstorm.config.saved_seeds.list
  for _, s in ipairs(list) do
    if s.seed == seed then
      saveManagerAlert("Seed " .. seed .. " is already saved")
      return
    end
  end
  if #list >= ST.MAX_SEEDS then
    saveManagerAlert("Saved seeds are full (" .. ST.MAX_SEEDS .. "): delete some first")
    return
  end
  table.insert(list, 1, { seed = seed, note = "" })
  Brainstorm.writeConfig()
  saveManagerAlert("Saved seed " .. seed)
  if seeds_shown() then
    ST.seed_page = 1
    ST.show("seeds")
  end
end

-- Starts a new run on saved seed i with the chosen deck and stake.
function ST.play(i)
  local ss = Brainstorm.config.saved_seeds
  local s = ss.list[i]
  if not s then
    return
  end
  Brainstorm.writeConfig() -- keep a note being typed
  local deck = G.P_CENTERS[ss.deck] or G.P_CENTERS.b_red
  if Brainstorm.ar_job then
    Brainstorm.stopAutoReroll()
  end
  if G.OVERLAY_MENU then
    G.FUNCS.exit_overlay_menu()
  end
  -- start_run takes the deck from viewed_back (as the run setup screen
  -- leaves it) and the stake and seed from its arguments.
  G.GAME.viewed_back = Back(deck)
  G.FUNCS.start_run(nil, { stake = ss.stake, seed = s.seed })
end

G.FUNCS.bs_st_save_seed = function()
  ST.save_current_seed()
end
G.FUNCS.bs_st_deck = function(args)
  local c = ST.deck_list and ST.deck_list[args.to_key]
  if c then
    Brainstorm.config.saved_seeds.deck = c.key
    Brainstorm.writeConfig()
  end
end
G.FUNCS.bs_st_stake = function(args)
  Brainstorm.config.saved_seeds.stake = args.to_key
  Brainstorm.writeConfig()
end
G.FUNCS.bs_st_seed_page = function(args)
  ST.seed_page = args.to_key
  ST.confirm_delete, ST.confirm_play = nil, nil
  ST.show("seeds")
end
for slot = 1, ST.SEEDS_PER_PAGE do
  G.FUNCS["bs_st_play_" .. slot] = function()
    local i = (ST.seed_page - 1) * ST.SEEDS_PER_PAGE + slot
    ST.confirm_delete = nil
    -- In a run, Play ends it: ask first.
    if G.STAGE == G.STAGES.RUN and ST.confirm_play ~= i then
      ST.confirm_play = i
      ST.show("seeds")
      return
    end
    ST.confirm_play = nil
    ST.play(i)
  end
  G.FUNCS["bs_st_del_" .. slot] = function()
    local i = (ST.seed_page - 1) * ST.SEEDS_PER_PAGE + slot
    ST.confirm_play = nil
    if ST.confirm_delete ~= i then
      ST.confirm_delete = i
    else
      ST.confirm_delete = nil
      table.remove(Brainstorm.config.saved_seeds.list, i)
      Brainstorm.writeConfig()
    end
    ST.show("seeds")
  end
end

-- ---------------------------------------------------------------------------
-- Hotkeys

-- Called by Controller:key_press_update (Core/Brainstorm.lua) for the next
-- key pressed after a hotkey button was clicked, instead of the game.
function ST.capture(key)
  local action = ST.capturing
  ST.capturing = nil
  local kb = Brainstorm.config.keybinds
  if key ~= "escape" then
    local problem
    if SAVE_SLOT_KEYS[key] then
      problem = "1-5 pick the save state slot"
    else
      for _, h in ipairs(ST.HOTKEYS) do
        if h[1] ~= action and kb[h[1]] == key then
          problem = ST.key_name(key) .. " is already used for: " .. h[2]:gsub("^%+ ", "")
        end
      end
    end
    if problem then
      saveManagerAlert(problem)
    else
      kb[action] = key
      Brainstorm.writeConfig()
    end
  end
  ST.show("hotkeys")
end

for i, h in ipairs(ST.HOTKEYS) do
  G.FUNCS["bs_st_key_" .. i] = function()
    ST.capturing = h[1]
    ST.show("hotkeys")
  end
end
G.FUNCS.bs_st_keys_reset = function()
  for k, v in pairs(ST.DEFAULT_KEYS) do
    Brainstorm.config.keybinds[k] = v
  end
  Brainstorm.writeConfig()
  ST.show("hotkeys")
end

-- The part buttons.
for _, sub in ipairs(ST.SUBS) do
  G.FUNCS["bs_st_sub_" .. sub[1]] = function()
    ST.capturing, ST.confirm_delete, ST.confirm_play = nil, nil, nil
    Brainstorm.FB.confirm_delete = nil
    Brainstorm.MB.confirm_load = nil
    ST.show(sub[1])
  end
end

return ST
