// filters.hpp — filter configuration and the lookup tables that turn the
// strings sent from Lua into Immolate's Item enum values.
#ifndef FILTERS_HPP
#define FILTERS_HPP

#include "instance.hpp"
#include <memory>
#include <string>

class FilterProgram; // filterprog.hpp

// Custom filters: hand-written C++ filters selectable by name, which
// override the regular menu options. The original four (Negative Blueprint,
// Negative Perkeo, N_Perk_Blueprint, Perkeo Baseball) were removed in
// October 2026; a new custom-filter system is planned. Until then only
// NO_FILTER exists, and any other name is treated as NO_FILTER so that old
// saved settings keep working.
enum class customFilters {
  NO_FILTER,
};

inline customFilters stringToFilter(const std::string &) {
  return customFilters::NO_FILTER;
}

// Everything brainstormFilter() needs to know. Built once per search from the
// bs_params the Lua side sent (see brainstorm_api.cpp), then shared read-only
// by all worker threads.
struct FilterConfig {
  Item tag = Item::RETRY;     // RETRY means "any"
  Item voucher = Item::RETRY;
  Item pack = Item::RETRY;
  int souls = 0;
  bool observatory = false;
  bool perkeo = false;
  bool copymoney = false;
  bool retcon = false;
  bool bean = false;
  bool burglar = false;
  customFilters custom = customFilters::NO_FILTER;
  // Custom filter built in game (filterprog.hpp), or null. Checked after
  // all the options above; read-only, shared by every worker thread.
  std::shared_ptr<const FilterProgram> program;
};

long brainstormFilter(Instance inst, const FilterConfig &cfg);

// Converts a game key ("tag_charm", "v_telescope", "p_arcana_mega") to an
// Item. Also accepts Immolate's English names ("Charm Tag") for backwards
// compatibility. Empty string -> Item::RETRY ("don't filter").
// Sets `ok` to false if the string is non-empty but not recognised.
Item itemFromKey(const std::string &key, bool &ok);

// The reverse for tags, vouchers and packs: the game key of `item`, or ""
// if it has none in the table.
std::string keyFromItem(Item item);

#endif // FILTERS_HPP
