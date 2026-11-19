-- mobile.lua — touch controls, behind the "Mobile Friendly" setting.
--
-- Every Brainstorm-Rerolled action has a hotkey, which a phone can't press.
-- With Mobile Friendly on (Brainstorm.config.mobile):
--   * the "Mobile" part of the Settings tab (UI/settings.lua) has buttons
--     for search, reroll, saving the seed, and the five save state slots;
--   * during a run, a small "Brainstorm" button at the right edge of the
--     screen opens that part directly.
-- With it off, the part only shows the toggle and what it does.
--
-- The setting starts on for Android and off elsewhere (first start only;
-- after that it is the player's choice).

local MB = {}
Brainstorm.MB = MB

MB.SLOTS = { "1", "2", "3", "4", "5" }
MB.confirm_load = nil -- slot waiting for a second tap on Load (in a run)

function MB.is_android()
  return love.system.getOS() == "Android"
end

function MB.on()
  return Brainstorm.config.mobile == true
end

function MB.ensure_config()
  if Brainstorm.config.mobile == nil then
    Brainstorm.config.mobile = MB.is_android()
  end
end

local function text(str, scale, colour)
  return { n = G.UIT.T, config = { text = str, scale = scale or 0.35, colour = colour or G.C.UI.TEXT_LIGHT } }
end
local function text_row(str, scale, colour)
  return { n = G.UIT.R, config = { align = "cm", padding = 0.03 }, nodes = { text(str, scale, colour) } }
end
-- Bigger than the other Settings buttons: they are tapped with a finger.
local function button(label, func, colour, minw)
  return UIBox_button({ label = { label }, button = func, colour = colour, minw = minw or 2, minh = 0.6, scale = 0.38, col = true })
end
local function panel(nodes)
  return {
    n = G.UIT.R,
    config = { align = "cm", padding = 0.08, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
    nodes = nodes,
  }
end

local function in_run()
  return G.STAGE == G.STAGES.RUN
end

-- The "Mobile" part of the Settings tab.
function MB.rows()
  local rows = {
    { n = G.UIT.R, config = { align = "cm" }, nodes = {
      create_toggle({
        label = "Mobile Friendly",
        ref_table = Brainstorm.config,
        ref_value = "mobile",
        callback = MB.toggled,
      }),
    } },
  }
  if not MB.on() then
    rows[#rows + 1] = text_row("Adds buttons for everything the hotkeys do: search, reroll,", 0.32, G.C.UI.TEXT_INACTIVE)
    rows[#rows + 1] = text_row("save this seed and save states, plus a Brainstorm button", 0.32, G.C.UI.TEXT_INACTIVE)
    rows[#rows + 1] = text_row("during runs that opens them.", 0.32, G.C.UI.TEXT_INACTIVE)
    return rows
  end

  local active = Brainstorm.FB.active()
  rows[#rows + 1] = text_row("Filter: " .. (active and active.name or "None (pick one in Filters)"), 0.34)
  rows[#rows + 1] = panel({
    button(Brainstorm.ar_job and "Stop search" or "Search", "bs_mb_search", Brainstorm.ar_job and G.C.RED or G.C.GREEN, 2.4),
    button("Reroll", "bs_mb_reroll", G.C.BLUE, 2.4),
    in_run() and button("Save this seed", "bs_mb_save_seed", G.C.BLUE, 2.8)
      or { n = G.UIT.C, config = { align = "cm", minw = 2.8 }, nodes = { text("(save seed: in a run)", 0.3, G.C.UI.TEXT_INACTIVE) } },
  })

  rows[#rows + 1] = text_row("SAVE STATES", 0.36)
  local slots = { n = G.UIT.R, config = { align = "cm", padding = 0.05 }, nodes = {} }
  for i, k in ipairs(MB.SLOTS) do
    local sure = MB.confirm_load == k
    slots.nodes[#slots.nodes + 1] = {
      n = G.UIT.C,
      config = { align = "cm", padding = 0.06, r = 0.1, colour = darken(G.C.UI.TRANSPARENT_DARK, 0.2) },
      nodes = {
        text_row("Slot " .. k, 0.34),
        { n = G.UIT.R, config = { align = "cm" }, nodes = {
          in_run() and button("Save", "bs_mb_save_" .. i, G.C.BLUE, 1.7)
            or { n = G.UIT.C, config = { align = "cm", minw = 1.7, minh = 0.6 }, nodes = { text("-", 0.34, G.C.UI.TEXT_INACTIVE) } },
        } },
        { n = G.UIT.R, config = { align = "cm" }, nodes = {
          button(sure and "Sure?" or "Load", "bs_mb_load_" .. i, sure and G.C.RED or G.C.ORANGE, 1.7),
        } },
      },
    }
  end
  rows[#rows + 1] = slots
  rows[#rows + 1] = text_row("Saving needs a run. Loading ends the current run.", 0.3, G.C.UI.TEXT_INACTIVE)
  return rows
end

-- Closes the Settings window, so what happens next can be seen.
local function close_menu()
  if G.OVERLAY_MENU then
    G.FUNCS.exit_overlay_menu()
  end
end

G.FUNCS.bs_mb_search = function()
  close_menu()
  Brainstorm.toggleAutoReroll()
end
G.FUNCS.bs_mb_reroll = function()
  close_menu()
  if Brainstorm.ar_job then
    Brainstorm.stopAutoReroll()
  end
  Brainstorm.reroll()
end
G.FUNCS.bs_mb_save_seed = function()
  close_menu()
  Brainstorm.ST.save_current_seed()
end
for i, k in ipairs(MB.SLOTS) do
  G.FUNCS["bs_mb_save_" .. i] = function()
    MB.confirm_load = nil
    close_menu()
    Brainstorm.saveState(k)
  end
  G.FUNCS["bs_mb_load_" .. i] = function()
    -- In a run, loading ends it: ask first.
    if in_run() and MB.confirm_load ~= k then
      MB.confirm_load = k
      Brainstorm.ST.show("mobile")
      return
    end
    MB.confirm_load = nil
    close_menu()
    Brainstorm.loadState(k)
  end
end

-- ---------------------------------------------------------------------------
-- The in-run button

-- Shows or removes it to match the setting. It belongs to the run: the game
-- removes everything made during a run when the run ends (Game:delete_run).
function MB.update_run_button()
  if MB.run_button and not MB.run_button.REMOVED then
    MB.run_button:remove()
  end
  MB.run_button = nil
  if not (MB.on() and in_run() and G.ROOM_ATTACH) then
    return
  end
  MB.run_button = UIBox({
    definition = {
      n = G.UIT.ROOT,
      config = { align = "cm", colour = G.C.CLEAR },
      nodes = {
        UIBox_button({ label = { "Brainstorm" }, button = "bs_mb_open", colour = G.C.BLUE, minw = 1.9, minh = 0.6, scale = 0.38 }),
      },
    },
    -- Right edge, middle: between the consumable slots and the deck, where
    -- the game puts nothing.
    config = { align = "cri", offset = { x = -0.15, y = 0 }, major = G.ROOM_ATTACH },
  })
end

G.FUNCS.bs_mb_open = function()
  Brainstorm.ST.sub = "mobile"
  Brainstorm.ST.open()
end

function MB.toggled()
  MB.confirm_load = nil
  Brainstorm.writeConfig()
  MB.update_run_button()
  Brainstorm.ST.show("mobile")
end

local start_run_ref = Game.start_run
function Game:start_run(args)
  local r = start_run_ref(self, args)
  MB.update_run_button()
  return r
end

return MB
