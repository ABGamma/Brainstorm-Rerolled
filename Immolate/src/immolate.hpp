
#include <string>
#ifdef _WIN32
#ifdef BUILDING_DLL
#define IMMOLATE_API __declspec(dllexport)
#else
#define IMMOLATE_API __declspec(dllimport)
#endif
#else
#define IMMOLATE_API
#endif

enum class customFilters {
    NO_FILTER,
    NEGATIVE_BLUEPRINT,
    NEGATIVE_PERKEO,
	NEGATIVE_PERKEO_BLUEPRINT,
    PERKEO_BASEBALL,
};

inline std::string filterToString(customFilters f) {
       switch (f) {
        case customFilters::NO_FILTER:
            return "No Filter";
        case customFilters::NEGATIVE_BLUEPRINT:
            return "Negative Blueprint";
		case customFilters::NEGATIVE_PERKEO:
			return "Negative Perkeo";
		case customFilters::NEGATIVE_PERKEO_BLUEPRINT:
			return "N_Perk_Blueprint";
        case customFilters::PERKEO_BASEBALL:
			return "Perkeo Baseball";
        default:
            return "Unknown Filter";
	   }
}

inline customFilters stringToFilter(std::string i) {
    if (i == "No Filter") return customFilters::NO_FILTER;
    // Add more cases for other items as needed
	if (i == "Negative Blueprint") return customFilters::NEGATIVE_BLUEPRINT;
	if (i == "Negative Perkeo") return customFilters::NEGATIVE_PERKEO;
	if (i == "N_Perk_Blueprint") return customFilters::NEGATIVE_PERKEO_BLUEPRINT;
	if (i == "Perkeo Baseball") return customFilters::PERKEO_BASEBALL;
    return customFilters::NO_FILTER; // Default case
}

// Declare the functions with IMMOLATE_API
IMMOLATE_API std::string brainstorm_cpp(std::string seed, std::string voucher, std::string pack, std::string tag, double souls, bool observatory, bool perkeo, bool copymoney, bool retcon, bool bean, bool burglar, std::string customFilter);
extern "C" {
    IMMOLATE_API const char* brainstorm(const char* seed, const char* voucher, const char* pack, const char* tag, double souls, bool observatory, bool perkeo, bool copymoney, bool retcon, bool bean, bool burglar, const char* customFilter);
    IMMOLATE_API void free_result(const char* result);
}

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
EMSCRIPTEN_BINDINGS(Immolate) {
  // instance.hpp
  register_vector<std::string>("VectorStr");
  register_vector<JokerData>("VectorJkr");
  register_vector<Card>("VectorCrd");
  class_<InstParams>("InstParams")
      .constructor<>()
      .constructor<std::string, std::string, bool, long>()
      .property("deck", &InstParams::deck)
      .property("stake", &InstParams::stake)
      .property("showman", &InstParams::showman)
      .property("vouchers", &InstParams::vouchers)
      .property("version", &InstParams::version);
  class_<Instance>("Instance")
      .constructor<std::string>()
      .function("get_node", &Instance::get_node)
      .function("random", &Instance::random)
      .function("randint", &Instance::randint)
      .function("randchoice", &Instance::randchoice)
      .property("params", &Instance::params)
      .property("seed", &Instance::seed)

      // functions.hpp
      .function("lock", &Instance::lock)
      .function("unlock", &Instance::unlock)
      .function("isLocked", &Instance::isLocked)
      .function("initLocks", &Instance::initLocks)
      .function("initUnlocks", &Instance::initUnlocks)
      .function("nextTarot", &Instance::nextTarot)
      .function("nextPlanet", &Instance::nextPlanet)
      .function("nextSpectral", &Instance::nextSpectral)
      .function("nextJoker", &Instance::nextJoker)
      .function("getShopInstance", &Instance::getShopInstance)
      .function("nextShopItem", &Instance::nextShopItem)
      .function("nextPack", &Instance::nextPack)
      .function("nextStandardCard", &Instance::nextStandardCard)
      .function("nextArcanaPack", &Instance::nextArcanaPack)
      .function("nextCelestialPack", &Instance::nextCelestialPack)
      .function("nextSpectralPack", &Instance::nextSpectralPack)
      .function("nextBuffoonPack", &Instance::nextBuffoonPack)
      .function("nextStandardPack", &Instance::nextStandardPack)
      .function("isVoucherActive", &Instance::isVoucherActive)
      .function("activateVoucher", &Instance::activateVoucher)
      .function("nextVoucher", &Instance::nextVoucher)
      .function("setDeck", &Instance::setDeck)
      .function("setStake", &Instance::setStake)
      .function("nextTag", &Instance::nextTag)
      .function("nextBoss", &Instance::nextBoss);
  function("packInfo", &packInfo);

  // items.hpp
  class_<ShopInstance>("ShopInstance")
      .constructor<>()
      .constructor<double, double, double, double, double>()
      .function("getTotalRate", &ShopInstance::getTotalRate)
      .property("jokerRate", &ShopInstance::jokerRate)
      .property("tarotRate", &ShopInstance::tarotRate)
      .property("planetRate", &ShopInstance::planetRate)
      .property("playingCardRate", &ShopInstance::playingCardRate)
      .property("spectralRate", &ShopInstance::spectralRate);
  class_<JokerStickers>("JokerStickers")
      .constructor<>()
      .constructor<bool, bool, bool>()
      .property("eternal", &JokerStickers::eternal)
      .property("perishable", &JokerStickers::perishable)
      .property("rental", &JokerStickers::rental);
  class_<JokerData>("JokerData")
      .constructor<>()
      .constructor<std::string, std::string, std::string, JokerStickers>()
      .property("joker", &JokerData::joker)
      .property("rarity", &JokerData::rarity)
      .property("edition", &JokerData::edition)
      .property("stickers", &JokerData::stickers);
  class_<ShopItem>("ShopItem")
      .constructor<>()
      .constructor<std::string, std::string>()
      .constructor<std::string, std::string, JokerData>()
      .property("type", &ShopItem::type)
      .property("item", &ShopItem::item)
      .property("jokerData", &ShopItem::jokerData);
  class_<WeightedItem>("WeightedItem")
      .constructor<std::string, double>()
      .property("item", &WeightedItem::item)
      .property("weight", &WeightedItem::weight);
  class_<Pack>("Pack")
      .constructor<std::string, int, int>()
      .property("type", &Pack::type)
      .property("size", &Pack::size)
      .property("choices", &Pack::choices);
  class_<Card>("Card")
      .constructor<std::string, std::string, std::string, std::string>()
      .property("base", &Card::base)
      .property("enhancement", &Card::enhancement)
      .property("edition", &Card::edition)
      .property("seal", &Card::seal);
  constant("ENHANCEMENTS", &ENHANCEMENTS);
  constant("CARDS", &CARDS);
  constant("SUITS", &SUITS);
  constant("RANKS", &RANKS);
  constant("TAROTS", &TAROTS);
  constant("PLANETS", &PLANETS);
  constant("COMMON_JOKERS", &COMMON_JOKERS);
  constant("UNCOMMON_JOKERS", &UNCOMMON_JOKERS);
  constant("RARE_JOKERS", &RARE_JOKERS);
  constant("LEGENDARY_JOKERS", &LEGENDARY_JOKERS);
  constant("VOUCHERS", &VOUCHERS);
  constant("SPECTRALS", &SPECTRALS);
  constant("TAGS", &TAGS);
  constant("BOSSES", &BOSSES);

  // util.hpp
  function("pseudohash", &pseudohash) class_<LuaRandom>("LuaRandom")
      .constructor<>()
      .constructor<double>()
      .function("random", &LuaRandom::random);
}
#endif