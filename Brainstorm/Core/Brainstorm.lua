local lovely = require("lovely")
local nfs = require("nativefs")

Brainstorm = {}

-- The mod's version. Keep it equal to VERSION in steamodded_compat.lua: the
-- release workflow (.github/workflows/build.yml) checks both and names the
-- release after it.
Brainstorm.VERSION = "2.1.0-alpha"

Brainstorm.SMODS = nil

local saveKeys = { "1", "2", "3", "4", "5" }

Brainstorm.config = {
  enable = true,
  keybind_autoreroll = "r",
  -- Hotkeys (changed in Settings > Brainstorm-Rerolled > Hotkeys; the
  -- defaults are Brainstorm.ST.DEFAULT_KEYS in UI/settings.lua).
  keybinds = {
    options = "t",
    modifier = "lctrl",
    f_reroll = "r",
    a_reroll = "a",
    save_seed = "s",
    s_state = "z",
    l_state = "x",
  },
  -- Saved seeds ({ list = { {seed, note}, ... }, deck, stake }); see
  -- UI/settings.lua.
  saved_seeds = nil,
  -- Touch buttons (UI/mobile.lua). nil until the first start, which turns
  -- it on for Android and off elsewhere.
  mobile = nil,
  -- Saved filters ({ active = n, list = { {name, assume_enhanced,
  -- conditions}, ... } }); created and upgraded by
  -- Brainstorm.FB.ensure_config() in UI/filter_builder.lua.
  filters = nil,
}

-- Auto-reroll state.
--   ar_job     handle of the running native search, or nil when idle
--   ar_status  text shown on screen while searching (DynaText reads it
--              every frame through ref_table/ref_value)
--   ar_text    the on-screen text object, so it can be removed afterwards
Brainstorm.ar_job = nil
Brainstorm.ar_status = ""
Brainstorm.ar_text = nil
Brainstorm.ar_started_at = 0

-- The native searcher wrapper; filled in by Brainstorm.init().
Brainstorm.native = nil

-- Cache frequently used functions
local math_abs = math.abs
local string_format = string.format
local string_lower = string.lower

-- This mod's folder inside the Mods folder. Its name varies:
-- "Brainstorm-Rerolled" for a manual install (or "Brainstorm" from older
-- versions), "ABGamma-Brainstorm_Rerolled" or similar from a mod
-- manager. Checking for Core/native.lua skips other mods with "brainstorm"
-- in their folder name (e.g. the original Brainstorm).
local function findBrainstormDirectory(directory)
  for _, item in ipairs(nfs.getDirectoryItems(directory)) do
    local itemPath = directory .. "/" .. item
    if
      nfs.getInfo(itemPath, "directory")
      and string_lower(item):find("brainstorm")
      and nfs.getInfo(itemPath .. "/Core/native.lua")
    then
      return itemPath
    end
  end
  return nil
end

local function fileExists(filePath)
  return nfs.getInfo(filePath) ~= nil
end

function Brainstorm.loadConfig()
  local configPath = Brainstorm.PATH .. "/config.lua"
  if not fileExists(configPath) then
    Brainstorm.writeConfig()
  else
    local configFile, err = nfs.read(configPath)
    if not configFile then
      error("Failed to read config file: " .. (err or "unknown error"))
    end
    Brainstorm.config = STR_UNPACK(configFile) or Brainstorm.config
  end
end

function Brainstorm.writeConfig()
  local configPath = Brainstorm.PATH .. "/config.lua"
  local success, err = nfs.write(configPath, STR_PACK(Brainstorm.config))
  if not success then
    error("Failed to write config file: " .. (err or "unknown error"))
  end
end

function Brainstorm.init()
  Brainstorm.PATH = findBrainstormDirectory(lovely.mod_dir)
  Brainstorm.loadConfig()
  assert(load(nfs.read(Brainstorm.PATH .. "/UI/ui.lua")))()
  -- The in-game custom filter builder (Brainstorm.FB).
  assert(load(nfs.read(Brainstorm.PATH .. "/UI/filter_builder.lua")))()
  Brainstorm.FB.ensure_config()
  -- The Settings tab: filter list, saved seeds, hotkeys (Brainstorm.ST).
  assert(load(nfs.read(Brainstorm.PATH .. "/UI/settings.lua")))()
  Brainstorm.ST.ensure_config()
  -- Touch controls (Brainstorm.MB): the "Mobile Friendly" setting.
  assert(load(nfs.read(Brainstorm.PATH .. "/UI/mobile.lua")))()
  Brainstorm.MB.ensure_config()
  -- Reads the running game's item lists for the searcher (Brainstorm.gamedata).
  Brainstorm.gamedata = assert(load(nfs.read(Brainstorm.PATH .. "/Core/gamedata.lua")))()

  -- Load the native searcher. If it fails (missing file, unsupported OS),
  -- everything except auto-reroll keeps working.
  Brainstorm.native = assert(load(nfs.read(Brainstorm.PATH .. "/Core/native.lua")))()
  local ok, err = Brainstorm.native.load(Brainstorm.PATH)
  if ok then
    print("[Brainstorm-Rerolled] " .. Brainstorm.native.build_info())
  else
    Brainstorm.native_error = err
    print("[Brainstorm-Rerolled] auto-reroll unavailable: " .. err)
  end
end

local key_press_update_ref = Controller.key_press_update
function Controller:key_press_update(key, dt)
    -- A hotkey button was clicked (Settings > Hotkeys): this key is its new
    -- key, and nothing else (Esc must not close the menu).
    if Brainstorm.ST and Brainstorm.ST.capturing then
        Brainstorm.ST.capture(key)
        return
    end
    local keybinds = Brainstorm.config.keybinds
    -- While a text field has the keyboard (a filter name, a seed), keys are
    -- text, not hotkeys: typing "x1" must not load save slot 1. Checked
    -- before the game handles the key, which may close the field.
    local typing = self.text_input_hook ~= nil
    key_press_update_ref(self, key, dt)
    if typing then
        return
    end
    for _, k in ipairs(saveKeys) do
        if key == k and love.keyboard.isDown(keybinds.s_state) then
            Brainstorm.saveState(k)
        end
        if key == k and love.keyboard.isDown(keybinds.l_state) then
            Brainstorm.loadState(k)
        end
    end
  
    if love.keyboard.isDown(keybinds.modifier) then
        if key == keybinds.f_reroll then
            Brainstorm.reroll()
        elseif key == keybinds.a_reroll then
            Brainstorm.toggleAutoReroll()
        elseif key == keybinds.options then
            Brainstorm.ST.open()
        elseif key == keybinds.save_seed then
            Brainstorm.ST.save_current_seed()
        end
    end
end

-- Save states: slot k ("1".."5") holds a copy of the run's latest autosave,
-- in the profile folder as saveState<k>.jkr. Used by the hotkeys above and
-- the Mobile Friendly buttons (UI/mobile.lua).
local function stateFile(k)
  return G.SETTINGS.profile .. "/" .. "saveState" .. k .. ".jkr"
end

function Brainstorm.saveState(k)
  if G.STAGE ~= G.STAGES.RUN then
    saveManagerAlert("Start a run to save its state")
  elseif G.ARGS.save_run == nil then
    -- G.ARGS.save_run is the game's latest autosave of this run; it is nil
    -- until the first one (saving nil would crash).
    saveManagerAlert("Nothing to save yet")
  else
    compress_and_save(stateFile(k), G.ARGS.save_run)
    saveManagerAlert("Saved state to slot [" .. k .. "]")
  end
end

function Brainstorm.loadState(k)
  -- Read the slot before touching the current run: an empty slot used to
  -- delete the run and start a new one (start_run with no savetext means
  -- "new run").
  local saved = get_compressed(stateFile(k))
  if saved == nil then
    saveManagerAlert("Slot [" .. k .. "] is empty")
    return
  end
  G:delete_run()
  G.SAVED_GAME = STR_UNPACK(saved)
  G:start_run({
    savetext = G.SAVED_GAME,
  })
  saveManagerAlert("Loaded save from slot [" .. k .. "]")
end

-- Starts a search, or cancels the running one.
function Brainstorm.toggleAutoReroll()
  if Brainstorm.ar_job then
    Brainstorm.stopAutoReroll("Search cancelled")
  else
    Brainstorm.startAutoReroll()
  end
end

function saveManagerAlert(text)
G.E_MANAGER:add_event(Event({
  trigger = "after",
  delay = 0.4,
  func = function()
  attention_text({
    text = text,
    scale = 0.7,
    hold = 3,
    major = G.STAGE == G.STAGES.RUN and G.play or G.title_top,
    backdrop_colour = G.C.SECONDARY_SET.Tarot,
    align = "cm",
    offset = {
      x = 0,
      y = -3.5,
    },
    silent = true,
  })
  G.E_MANAGER:add_event(Event({
    trigger = "after",
    delay = 0.06 * G.SETTINGS.GAMESPEED,
    blockable = false,
    blocking = false,
    func = function()
    play_sound("other1", 0.76, 0.4)
    return true
    end,
  }))
  return true
  end,
}))
end

function Brainstorm.reroll()
  local G = G -- Cache global G for performance
  G.GAME.viewed_back = nil
  G.run_setup_seed = G.GAME.seeded
  G.challenge_tab = G.GAME and G.GAME.challenge and G.GAME.challenge_tab or nil
  G.forced_seed = G.GAME.seeded and G.GAME.pseudorandom.seed or nil

  local seed = G.run_setup_seed and G.setup_seed or G.forced_seed
  local stake = (
    G.GAME.stake
    or G.PROFILES[G.SETTINGS.profile].MEMORY.stake
    or 1
  ) or 1

  G:delete_run()
  G:start_run({ stake = stake, seed = seed, challenge = G.challenge_tab })
end

-- ---------------------------------------------------------------------------
-- Auto-reroll
--
-- The search runs in native background threads. The game keeps running
-- normally; Game:update below checks on the search once per frame
-- (Brainstorm.pollAutoReroll) and starts the run as soon as a seed is found.
-- ---------------------------------------------------------------------------

-- The running game's item lists as text for the native searcher (see
-- Core/gamedata.lua): what makes searches right for a modded game, another
-- deck or a challenge. nil if they can't be read; the library then uses its
-- built-in lists of the unmodded game, and Brainstorm.game_data_error says
-- why.
function Brainstorm.gameData()
  if not Brainstorm.gamedata then
    return nil
  end
  local ok, text, err = pcall(Brainstorm.gamedata.get)
  if not ok then
    text, err = nil, tostring(text)
  end
  Brainstorm.game_data_error = (not text) and err or nil
  return text
end

-- What native.start() needs: where to start, and the active filter.
function Brainstorm.searchOptions()
  return {
    -- Start from a random point so repeated searches give different seeds.
    -- The clock is part of it: the cursor alone repeats when the mouse
    -- hasn't moved since the last search.
    start_seed = random_string(
      8,
      G.CONTROLLER.cursor_hover.T.x * 0.33411983
        + G.CONTROLLER.cursor_hover.T.y * 0.874146
        + 0.412311010 * G.CONTROLLER.cursor_hover.time
        + (os.time() % 100003) * 0.000731
        + (love.timer.getTime() % 1) * 0.37
    ),
    -- The active saved filter as text ("" = none; see UI/filter_builder.lua).
    filter_spec = Brainstorm.FB.spec(),
    game_data = Brainstorm.gameData(),
  }
end

-- "1234567" -> "1.2M"
local function shortNumber(n)
  if n >= 1e9 then
    return string_format("%.1fB", n / 1e9)
  elseif n >= 1e6 then
    return string_format("%.1fM", n / 1e6)
  elseif n >= 1e3 then
    return string_format("%.0fK", n / 1e3)
  end
  return tostring(n)
end

function Brainstorm.startAutoReroll()
  if not Brainstorm.native or not Brainstorm.native.lib then
    saveManagerAlert("Auto-reroll unavailable: " .. tostring(Brainstorm.native_error))
    return
  end
  local opts = Brainstorm.searchOptions()
  -- Say once (per reason) that the game's own lists couldn't be read.
  local why = not opts.game_data and Brainstorm.game_data_error
  -- A game with Steamodded's object weights (Pokermon, ...) picks items in
  -- another way altogether: the built-in lists would be wrong for nearly
  -- every seed, so without the game's own lists there is no search.
  if why and SMODS and SMODS.optional_features and SMODS.optional_features.object_weights then
    saveManagerAlert("Can't search this modded game: " .. why)
    return
  end
  if why and Brainstorm.game_data_warned ~= why then
    Brainstorm.game_data_warned = why
    saveManagerAlert("Using built-in item lists: " .. why)
  end
  local job, err = Brainstorm.native.start(opts)
  if not job then
    saveManagerAlert("Search failed: " .. tostring(err))
    return
  end
  Brainstorm.ar_job = job
  Brainstorm.ar_opts = opts
  Brainstorm.ar_started_at = love.timer.getTime()
  Brainstorm.ar_status = "Searching..."
  Brainstorm.ar_text = Brainstorm.attentionText({
    scale = 1.0,
    text = { { ref_table = Brainstorm, ref_value = "ar_status" } },
    align = "cm",
    offset = { x = 0, y = -3.5 },
    major = G.STAGE == G.STAGES.RUN and G.play or G.title_top,
  })
end

-- Ends the current search (if any) and removes the on-screen text.
function Brainstorm.stopAutoReroll(message)
  if Brainstorm.ar_job then
    Brainstorm.native.free(Brainstorm.ar_job)
    Brainstorm.ar_job = nil
  end
  if Brainstorm.ar_text then
    Brainstorm.removeAttentionText(Brainstorm.ar_text)
    Brainstorm.ar_text = nil
  end
  if message then
    saveManagerAlert(message)
  end
end

-- Called every frame while a search is running.
function Brainstorm.pollAutoReroll()
  local status, seed, checked = Brainstorm.native.poll(Brainstorm.ar_job)
  if status == "running" then
    local elapsed = love.timer.getTime() - Brainstorm.ar_started_at
    local rate = elapsed > 0 and checked / elapsed or 0
    Brainstorm.ar_status = string_format(
      "Searching... %s seeds (%s/s)",
      shortNumber(checked),
      shortNumber(math.floor(rate))
    )
  elseif status == "found" then
    Brainstorm.stopAutoReroll()
    Brainstorm.startFoundRun(seed)
  elseif status == "exhausted" then
    Brainstorm.stopAutoReroll("No seed matches these filters")
  else
    Brainstorm.stopAutoReroll("Search stopped (" .. status .. ")")
  end
end

-- Starts a new run on the seed the search found.
function Brainstorm.startFoundRun(seed)
  local stake = G.GAME.stake
  G:delete_run()
  G:start_run({
    stake = stake,
    seed = seed,
    challenge = G.GAME and G.GAME.challenge and G.GAME.challenge_tab,
  })
  -- Marks the seed as "found by filter" (shown in blue on the run screen)
  -- and makes the run count as unseeded, as it was before.
  G.GAME.used_filter = true
  -- (Not the whole ar_opts: the game's item lists in it are long, and this
  -- table is written into every save of the run.)
  local opts = Brainstorm.ar_opts or {}
  G.GAME.filter_info = {
    seed = seed,
    filter_params = { start_seed = opts.start_seed, filter_spec = opts.filter_spec },
  }
  G.GAME.seeded = false
  Brainstorm.checkPrediction(seed)
end

-- Compares what the run that has just started shows on ante 1 (skip tags,
-- shop voucher, boss) with what the searcher predicted for its seed. They
-- only differ when the game generates things differently from the lists it
-- reported, e.g. a mod that changes how items are picked; the filter may
-- then not hold for this seed, and the player is told.
-- Returns the list of things that differ (empty if all is well), or nil if
-- nothing could be compared.
function Brainstorm.checkPrediction(seed)
  local native, opts = Brainstorm.native, Brainstorm.ar_opts
  if not (native and native.lib and opts) then
    return nil
  end
  local ok, predicted = pcall(native.predict_start, { game_data = opts.game_data }, seed)
  if not ok or type(predicted) ~= "table" then
    return nil
  end
  local gm = G.GAME
  local resets = gm.round_resets or {}
  local voucher = gm.current_round and gm.current_round.voucher
  if type(voucher) == "table" then
    voucher = voucher[1] -- Steamodded keeps a list (there can be several)
  end
  local actual = {
    small = resets.blind_tags and resets.blind_tags.Small,
    big = resets.blind_tags and resets.blind_tags.Big,
    voucher = voucher,
    boss = resets.blind_choices and resets.blind_choices.Boss,
  }
  local names = { small = "Small Blind tag", big = "Big Blind tag", voucher = "voucher", boss = "boss" }
  local wrong = {}
  for _, what in ipairs({ "small", "big", "voucher", "boss" }) do
    local p, a = predicted[what], actual[what]
    if p and p ~= "?" and type(a) == "string" and a ~= p then
      wrong[#wrong + 1] = names[what]
      print(string.format("[Brainstorm-Rerolled] seed %s: predicted %s %s, the game has %s", seed, names[what], p, a))
    end
  end
  if #wrong > 0 then
    saveManagerAlert("Prediction differs from the game (" .. table.concat(wrong, ", ") .. "): this seed may not match the filter")
  end
  return wrong
end

local update_ref = Game.update
function Game:update(dt)
  update_ref(self, dt)
  if Brainstorm.ar_job then
    Brainstorm.pollAutoReroll()
  end
  -- A note (Settings > Saved seeds) is saved when typing in it ends, also
  -- by clicking somewhere else.
  local typing = G.CONTROLLER and G.CONTROLLER.text_input_hook ~= nil
  if Brainstorm.was_typing and not typing then
    Brainstorm.writeConfig()
  end
  Brainstorm.was_typing = typing
end

local cursr = create_UIBox_round_scores_row
function create_UIBox_round_scores_row(score, text_colour)
  local ret = cursr(score, text_colour)
  ret.nodes[2].nodes[1].config.colour = (score == "seed" and G.GAME.seeded)
      and G.C.RED
    or (score == "seed" and G.GAME.used_filter) and G.C.BLUE
    or G.C.BLACK
  return ret
end

-- TODO: Rework attention text.
function Brainstorm.attentionText(args)
  args = args or {}
  args.text = args.text or "test"
  args.scale = args.scale or 1
  args.colour = copy_table(args.colour or G.C.WHITE)
  args.hold = (args.hold or 0) + 0.1 * G.SPEEDFACTOR
  args.pos = args.pos or { x = 0, y = 0 }
  args.align = args.align or "cm"
  args.emboss = args.emboss or nil

  args.fade = 1

  if args.cover then
    args.cover_colour = copy_table(args.cover_colour or G.C.RED)
    args.cover_colour_l = copy_table(lighten(args.cover_colour, 0.2))
    args.cover_colour_d = copy_table(darken(args.cover_colour, 0.2))
  else
    args.cover_colour = copy_table(G.C.CLEAR)
  end

  args.uibox_config = {
    align = args.align or "cm",
    offset = args.offset or { x = 0, y = 0 },
    major = args.cover or args.major or nil,
  }

  G.E_MANAGER:add_event(Event({
    trigger = "after",
    delay = 0,
    blockable = false,
    blocking = false,
    func = function()
      args.AT = UIBox({
        T = { args.pos.x, args.pos.y, 0, 0 },
        definition = {
          n = G.UIT.ROOT,
          config = {
            align = args.cover_align or "cm",
            minw = (args.cover and args.cover.T.w or 0.001)
              + (args.cover_padding or 0),
            minh = (args.cover and args.cover.T.h or 0.001)
              + (args.cover_padding or 0),
            padding = 0.03,
            r = 0.1,
            emboss = args.emboss,
            colour = args.cover_colour,
          },
          nodes = {
            {
              n = G.UIT.O,
              config = {
                draw_layer = 1,
                object = DynaText({
                  scale = args.scale,
                  string = args.text,
                  maxw = args.maxw,
                  colours = { args.colour },
                  float = true,
                  shadow = true,
                  silent = not args.noisy,
                  args.scale,
                  pop_in = 0,
                  pop_in_rate = 6,
                  rotate = args.rotate or nil,
                }),
              },
            },
          },
        },
        config = args.uibox_config,
      })
      args.AT.attention_text = true

      args.text = args.AT.UIRoot.children[1].config.object
      args.text:pulse(0.5)

      if args.cover then
        Particles(args.pos.x, args.pos.y, 0, 0, {
          timer_type = "TOTAL",
          timer = 0.01,
          pulse_max = 15,
          max = 0,
          scale = 0.3,
          vel_variation = 0.2,
          padding = 0.1,
          fill = true,
          lifespan = 0.5,
          speed = 2.5,
          attach = args.AT.UIRoot,
          colours = {
            args.cover_colour,
            args.cover_colour_l,
            args.cover_colour_d,
          },
        })
      end
      if args.backdrop_colour then
        args.backdrop_colour = copy_table(args.backdrop_colour)
        Particles(args.pos.x, args.pos.y, 0, 0, {
          timer_type = "TOTAL",
          timer = 5,
          scale = 2.4 * (args.backdrop_scale or 1),
          lifespan = 5,
          speed = 0,
          attach = args.AT,
          colours = { args.backdrop_colour },
        })
      end
      return true
    end,
  }))
  return args
end

function Brainstorm.removeAttentionText(args)
  G.E_MANAGER:add_event(Event({
    trigger = "after",
    delay = 0,
    blockable = false,
    blocking = false,
    func = function()
      if not args.start_time then
        args.start_time = G.TIMERS.TOTAL
        if args.text.pop_out then
          args.text:pop_out(2)
        end
      else
        --args.AT:align_to_attach()
        args.fade = math.max(0, 1 - 3 * (G.TIMERS.TOTAL - args.start_time))
        if args.cover_colour then
          args.cover_colour[4] = math.min(args.cover_colour[4], 2 * args.fade)
        end
        if args.cover_colour_l then
          args.cover_colour_l[4] = math.min(args.cover_colour_l[4], args.fade)
        end
        if args.cover_colour_d then
          args.cover_colour_d[4] = math.min(args.cover_colour_d[4], args.fade)
        end
        if args.backdrop_colour then
          args.backdrop_colour[4] = math.min(args.backdrop_colour[4], args.fade)
        end
        args.colour[4] = math.min(args.colour[4], args.fade)
        if args.fade <= 0 then
          args.AT:remove()
          return true
        end
      end
    end,
  }))
end
