--[[
  native.lua — the bridge between Lua and the native Immolate searcher.

  Balatro runs on LuaJIT, whose built-in FFI ("foreign function interface")
  can load a compiled C library and call its functions directly. This file:

    1. declares the C functions/structs (a copy of Immolate/src/brainstorm_api.h)
    2. picks the right library file for the current operating system
    3. wraps start / poll / cancel in friendly Lua functions

  Usage (see Brainstorm.lua):

    local native = assert(load(nfs.read(path .. "/Core/native.lua")))()
    local ok, err = native.load(Brainstorm.PATH)
    local job = native.start({ tag = "tag_charm", souls = 1, ... })
    -- every frame:
    local status, seed, checked = native.poll(job)   -- "running"/"found"/...
    native.free(job)                                  -- when finished
]]

local ffi = require("ffi")

local native = {}

-- Must match BS_API_VERSION in brainstorm_api.h (2: adds filter_spec).
native.API_VERSION = 2

-- ffi.cdef may only declare each type once per Lua state. If this file is
-- loaded twice (e.g. a mod reload), the second declaration would error, so
-- it is wrapped in pcall and failures are ignored.
pcall(
  ffi.cdef,
  [[
  typedef struct bs_params {
    int32_t api_version;
    const char *start_seed;
    int32_t threads;
    const char *tag;
    const char *voucher;
    const char *pack;
    int32_t souls;
    int32_t observatory;
    int32_t perkeo;
    int32_t copy_money;
    int32_t retcon;
    int32_t bean;
    int32_t burglar;
    const char *custom_filter;
    const char *filter_spec;
  } bs_params;

  typedef struct bs_job bs_job;

  int32_t bs_api_version(void);
  const char *bs_build_info(void);
  bs_job *bs_search_start(const bs_params *params);
  int32_t bs_search_poll(bs_job *job, char *seed_out, uint64_t *seeds_checked);
  void bs_search_cancel(bs_job *job);
  void bs_search_free(bs_job *job);
  const char *bs_last_error(void);
  int32_t bs_check_seed(const bs_params *params, const char *seed);
]]
)

-- Status codes returned by bs_search_poll, as readable strings.
local STATUS = { [0] = "running", "found", "exhausted", "cancelled", "error" }

-- Library file name for each operating system LuaJIT reports.
-- Linux/Steam Deck players normally run Balatro through Proton, where
-- ffi.os is "Windows" and the .dll is used.
local LIB_NAMES = {
  Windows = "Immolate.dll",
  OSX = "Immolate.dylib",
  Linux = "Immolate.so",
}

native.lib = nil

-- Loads the native library from the mod folder. Returns true, or false and
-- an error message. Safe to call more than once.
function native.load(mod_path)
  if native.lib then
    return true
  end
  local file = LIB_NAMES[ffi.os]
  if not file then
    return false, "unsupported operating system: " .. tostring(ffi.os)
  end
  local ok, lib = pcall(ffi.load, mod_path .. "/" .. file)
  if not ok then
    return false, "could not load " .. file .. ": " .. tostring(lib)
  end
  -- Catch an old library left over from a previous mod version: calling a
  -- function with a different signature would crash the game.
  local has_api, version = pcall(function()
    return lib.bs_api_version()
  end)
  if not has_api then
    return false, file .. " is too old (no bs_api_version); please reinstall"
  end
  if version ~= native.API_VERSION then
    return false,
      string.format(
        "%s has API version %d but the mod expects %d; please reinstall",
        file,
        version,
        native.API_VERSION
      )
  end
  native.lib = lib
  return true
end

function native.build_info()
  return native.lib and ffi.string(native.lib.bs_build_info()) or "not loaded"
end

-- Fills a bs_params struct from a plain Lua table. Missing fields default to
-- "off". The Lua strings stay referenced by `opts` for the duration of the
-- C call, which copies everything it needs before returning.
local function make_params(opts)
  local p = ffi.new("bs_params")
  p.api_version = native.API_VERSION
  p.start_seed = opts.start_seed or ""
  p.threads = opts.threads or 0
  p.tag = opts.tag or ""
  p.voucher = opts.voucher or ""
  p.pack = opts.pack or ""
  p.souls = opts.souls or 0
  p.observatory = opts.observatory and 1 or 0
  p.perkeo = opts.perkeo and 1 or 0
  p.copy_money = opts.copy_money and 1 or 0
  p.retcon = opts.retcon and 1 or 0
  p.bean = opts.bean and 1 or 0
  p.burglar = opts.burglar and 1 or 0
  p.custom_filter = opts.custom_filter or "No Filter"
  -- The custom filter as text (see Immolate/src/filterprog.hpp), or "".
  p.filter_spec = opts.filter_spec or ""
  return p
end

-- Starts a background search. Returns a job handle, or nil and an error.
function native.start(opts)
  if not native.lib then
    return nil, "native library not loaded"
  end
  local job = native.lib.bs_search_start(make_params(opts))
  if job == nil then
    return nil, ffi.string(native.lib.bs_last_error())
  end
  -- If Lua garbage-collects a job we forgot to free, free it then.
  return ffi.gc(job, native.lib.bs_search_free)
end

-- Reusable output buffers so polling every frame allocates nothing.
local seed_buf = ffi.new("char[9]")
local checked_buf = ffi.new("uint64_t[1]")

-- Returns status ("running", "found", "exhausted", "cancelled", "error"),
-- the seed string (only when "found"), and the number of seeds checked.
function native.poll(job)
  local code = native.lib.bs_search_poll(job, seed_buf, checked_buf)
  local seed = code == 1 and ffi.string(seed_buf) or nil
  return STATUS[code] or "error", seed, tonumber(checked_buf[0])
end

function native.cancel(job)
  native.lib.bs_search_cancel(job)
end

-- Stops the search and releases it. Do not use `job` afterwards.
function native.free(job)
  ffi.gc(job, nil) -- we are freeing it now; the GC must not do it again
  native.lib.bs_search_free(job)
end

-- Checks one seed without starting a search. Returns true/false, or nil and
-- an error message.
function native.check_seed(opts, seed)
  local r = native.lib.bs_check_seed(make_params(opts), seed)
  if r < 0 then
    return nil, ffi.string(native.lib.bs_last_error())
  end
  return r == 1
end

return native
