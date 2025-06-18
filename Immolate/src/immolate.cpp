#define IMMOLATE_API __declspec(dllexport)
#include "functions.hpp"
#include"immolate.hpp"
#include "search.hpp"
#include <cstring>

Item BRAINSTORM_PACK = Item::RETRY;
Item BRAINSTORM_TAG = Item::Charm_Tag;
Item BRAINSTORM_VOUCHER = Item::RETRY;
double BRAINSTORM_SOULS = 1;
bool BRAINSTORM_OBSERVATORY = false;
bool BRAINSTORM_PERKEO = false;
bool BRAINSTORM_EARLYCOPY = false; 
bool BRAINSTORM_RETCON = false;
bool BRAINSTORM_ANTE8_BEAN = false;
bool BRAINSTORM_ANTE6_BURGLAR = false;
customFilters BRAINSTORM_FILTER = customFilters::NO_FILTER;

long filter(Instance inst) {
    switch (BRAINSTORM_FILTER) {
        case customFilters::NO_FILTER: {
            if (BRAINSTORM_PACK != Item::RETRY) {
                inst.cache.generatedFirstPack = true; //we don't care about Pack 1
                if (inst.nextPack(1) != BRAINSTORM_PACK) {
                    return 0;
                }
            }

            if (BRAINSTORM_VOUCHER != Item::RETRY && !BRAINSTORM_OBSERVATORY) {
                if (inst.nextVoucher(1) != BRAINSTORM_VOUCHER) {
                    return 0;
                }
            }

            if (BRAINSTORM_TAG != Item::RETRY) {
                if (inst.nextTag(1) != BRAINSTORM_TAG) {
                    return 0;
                }
            }
            if (BRAINSTORM_SOULS > 0) {
                for (int i = 1; i <= BRAINSTORM_SOULS; i++) {
                    auto tarots = inst.nextArcanaPack(5, 1); //Mega Arcana Pack, assumed from a Charm Tag
                    bool found_soul = false;
                    bool found_perkeo = false;
                    for (int t = 0; t < 5; t++) {
                        if (tarots[t] == Item::The_Soul) {
                            found_soul = true;
                            if (BRAINSTORM_PERKEO && inst.nextJoker(ItemSource::Soul, 1, true).joker == Item::Perkeo) {
                                found_perkeo = true;
                                break;
                            }
                            break;
                        }
                    }
                    if (!found_soul) {
                        return 0;
                    }
                    if (BRAINSTORM_PERKEO && !found_perkeo) {
                        return 0; // If Perkeo is required but not found, return 0
                    }
                }
            }
            if (BRAINSTORM_OBSERVATORY) {
                if (inst.nextVoucher(1) == Item::Telescope) {
                    inst.activateVoucher(Item::Telescope);
                    if (inst.nextVoucher(2) != Item::Observatory) {
                        return 0;
                    }
                }
                else return 0;
            }
            if (BRAINSTORM_EARLYCOPY) {
                long score = 0;
                bool money = false;
                bool bstorm = false;
                bool bprint = false;
                Pack pack = packInfo(inst.nextPack(1));
                for (int p = 0; p <= 3; p++) {
                    if (pack.type == Item::Buffoon_Pack || pack.type == Item::Jumbo_Buffoon_Pack) {
                        auto packContents = inst.nextBuffoonPack(pack.size, 1);
                        for (int x = 0; x < pack.size; x++) {
                            if (packContents[x].joker == Item::Blueprint && !bprint) {
                                bprint = true;
                                break;
                            }
                            if ((packContents[x].joker == Item::Mail_In_Rebate || packContents[x].joker == Item::Reserved_Parking || packContents[x].joker == Item::Business_Card || packContents[x].joker == Item::To_Do_List || packContents[x].joker == Item::Midas_Mask || packContents[x].joker == Item::Trading_Card) && !money) {
                                money = true;
                                break;
                            }
                            if (packContents[x].joker == Item::Brainstorm && !bstorm) {
                                bstorm = true;
                                break;
                            }
                        }
                    }
                    if (pack.type == Item::Mega_Buffoon_Pack) {
                        auto packContents = inst.nextBuffoonPack(pack.size, 1);
                        for (int x = 0; x < pack.size; x++) {
                            if (packContents[x].joker == Item::Blueprint) {
                                bprint = true;
                            }
                            if ((packContents[x].joker == Item::Mail_In_Rebate || packContents[x].joker == Item::Reserved_Parking || packContents[x].joker == Item::Business_Card || packContents[x].joker == Item::To_Do_List || packContents[x].joker == Item::Midas_Mask || packContents[x].joker == Item::Trading_Card) && !money) {
                                money = true;
                            }
                            if (packContents[x].joker == Item::Brainstorm) {
                                bstorm = true;
                            }
                        }
                    }
                    pack = packInfo(inst.nextPack(1));
                }
                for (int i = 0; i < 4; i++) {
                    ShopItem item = inst.nextShopItem(1);
                    if (item.item == Item::Blueprint && !bprint && i > 1) {
                        bprint = true;
                    }
                    if (i > 1 && (item.item == Item::Mail_In_Rebate || item.item == Item::Reserved_Parking || item.item == Item::Business_Card || item.item == Item::To_Do_List || item.item == Item::Midas_Mask || item.item == Item::Trading_Card) && !money) {
                        money = true;
                    }
                    if (item.item == Item::Brainstorm && !bstorm && i > 1) {
                        bstorm = true;
                    }
                    if (!bprint || !money || !bstorm) {
                        return 0; // If any of the required items are not found, return 0
                    }


                }
            }
            if (BRAINSTORM_ANTE8_BEAN) {
                bool bean = false;
                for (int i = 0; i < 150; i++) {
                    if (inst.nextShopItem(8).item == Item::Turtle_Bean) {
                        bean = true;
                        break;
                    }
                }
                if (!bean) {
                    return 0; // If Turtle Bean is not found, return 0
                }
            }
            if (BRAINSTORM_ANTE6_BURGLAR) {
                bool burglar = false;
                for (int i = 0; i < 50; i++) {
                    if (inst.nextShopItem(6).item == Item::Burglar) {
                        burglar = true;
                        break;
                    }
                }
                if (!burglar) {
                    for (int i = 0; i < 100; i++) {
                        if (inst.nextShopItem(7).item == Item::Burglar) {
                            burglar = true;
                            break;
                        }
                    }
                }
                if (!burglar) {
                    for (int i = 0; i < 100; i++) {
                        if (inst.nextShopItem(8).item == Item::Burglar) {
                            burglar = true;
                            break;
                        }
                    }
                }
                if (!burglar) {
                    return 0; // If Burglar is not found, return 0
                }
            }
            if (BRAINSTORM_RETCON) {
                inst.initLocks(1, false, true);
                bool directorsCut = false;
                bool retcon = false;
                for (int i = 1; i < 9; i++) {
                    auto voucher = inst.nextVoucher(i);
                    inst.activateVoucher(voucher);
                    if (voucher == Item::Directors_Cut || directorsCut) {
                        directorsCut = true;
                        if (voucher == Item::Retcon) {
                            retcon = true;
                        }
                    }
                }
                if (!retcon) {
                    return 0; // If Retcon is not found, return 0
                }
            }

            return 1; // Return a score of 1 if all conditions are met
        }
        case customFilters::NEGATIVE_BLUEPRINT: {
            bool bprint = false;
            for (int i = 0; i < 4; i++) {
                ShopItem item = inst.nextShopItem(1);
                if (item.type == Item::Joker) {
                    if (item.jokerData.joker == Item::Blueprint && item.jokerData.edition == Item::Negative) {
                        bprint = true;
                    }
                }
            }
            if (bprint) {
                return 1; // Return a score of 1 if a negative blueprint is found
            }
            Pack pack = packInfo(inst.nextPack(1));
            for (int p = 0; p <= 3; p++) {
                if (pack.type == Item::Buffoon_Pack || pack.type == Item::Jumbo_Buffoon_Pack) {
                    auto packContents = inst.nextBuffoonPack(pack.size, 1);
                    for (int x = 0; x < pack.size; x++) {
                        if (packContents[x].joker == Item::Blueprint && packContents[x].edition == Item::Negative) {
                            bprint = true;
                            break;
                        }
                    }
                }
                if (pack.type == Item::Mega_Buffoon_Pack) {
                    auto packContents = inst.nextBuffoonPack(pack.size, 1);
                    for (int x = 0; x < pack.size; x++) {
                        if (packContents[x].joker == Item::Blueprint && packContents[x].edition == Item::Negative) {
                            bprint = true;
                        }
                    }
                }
                pack = packInfo(inst.nextPack(1));
            }
            if (bprint) {
                return 1; // Return a score of 1 if a negative blueprint is found
            }
            return 0; // Return 0 if no negative blueprint is found 
        }
        case customFilters::NEGATIVE_PERKEO: {
			bool negativePerkeo = false;
            if (inst.nextTag(1) != Item::Charm_Tag) {
                return 0;
            }
            auto tarots = inst.nextArcanaPack(5, 1); //Mega Arcana Pack, assumed from a Charm Tag
            for (int t = 0; t < 5; t++) {
                if (tarots[t] == Item::The_Soul) {
					auto nextJoker = inst.nextJoker(ItemSource::Soul, 1, true);
                    if (nextJoker.joker == Item::Perkeo && nextJoker.edition == Item::Negative) {
                        negativePerkeo = true;
                        break;
                    }
                    break;
                }
            }
            if (!negativePerkeo) {
                return 0; // If Perkeo is required but not found, return 0
            }
            
			return 1; // Return a score of 1 if a negative Perkeo is found

        }
        case customFilters::NEGATIVE_PERKEO_BLUEPRINT: {
            bool negativePerkeo = false;
            if (inst.nextTag(1) != Item::Charm_Tag) {
                return 0;
            }
            auto tarots = inst.nextArcanaPack(5, 1); //Mega Arcana Pack, assumed from a Charm Tag
            for (int t = 0; t < 5; t++) {
                if (tarots[t] == Item::The_Soul) {
                    auto nextJoker = inst.nextJoker(ItemSource::Soul, 1, true);
                    if (nextJoker.joker == Item::Perkeo && nextJoker.edition == Item::Negative) {
                        negativePerkeo = true;
                        break;
                    }
                    break;
                }
            }
            if (!negativePerkeo) {
                return 0; // If Perkeo is required but not found, return 0
			}
            bool bprint = false;
            for (int i = 0; i < 2; i++) {
                ShopItem item = inst.nextShopItem(1);
                if (item.type == Item::Joker) {
                    if (item.jokerData.joker == Item::Blueprint && item.jokerData.edition == Item::Negative) {
                        bprint = true;
                    }
                }
            }
            if (bprint) {
                return 1; // Return a score of 1 if a negative blueprint is found
            }
            Pack pack = packInfo(inst.nextPack(1));
            for (int p = 0; p <= 2; p++) {
                if (pack.type == Item::Buffoon_Pack || pack.type == Item::Jumbo_Buffoon_Pack || pack.type == Item::Mega_Buffoon_Pack) {
                    auto packContents = inst.nextBuffoonPack(pack.size, 1);
                    for (int x = 0; x < pack.size; x++) {
                        if (packContents[x].joker == Item::Blueprint && packContents[x].edition == Item::Negative) {
                            bprint = true;
                            break;
                        }
                    }
                }
                pack = packInfo(inst.nextPack(1));
            }
            if (bprint) {
                return 1; // Return a score of 1 if a negative blueprint is found
            }

            return 0; // Return 0 if no negative blueprint is found 
        }
        case customFilters::PERKEO_BASEBALL: {
            bool perkeo = false;
            if (inst.nextTag(1) != Item::Charm_Tag) {
                return 0;
            }
            auto tarots = inst.nextArcanaPack(5, 1); //Mega Arcana Pack, assumed from a Charm Tag
            for (int t = 0; t < 5; t++) {
                if (tarots[t] == Item::The_Soul) {
                    auto nextJoker = inst.nextJoker(ItemSource::Soul, 1, true);
                    if (nextJoker.joker == Item::Perkeo) {
                        perkeo = true;
                        break;
                    }
                    break;
                }
            }
            if (!perkeo) {
                return 0; // If Perkeo is required but not found, return 0
            }
            Pack pack = packInfo(inst.nextPack(1));
            auto packContents = inst.nextBuffoonPack(pack.size, 1);
            for (int x = 0; x < pack.size; x++) {
                if (packContents[x].joker == Item::Baseball_Card && packContents[x].edition == Item::Negative) {
					return 1; // Return a score of 1 if a negative Baseball Card is found
                }
            }
			return 0; // Return 0 if no negative Baseball Card is found
        }
        default:
            return 1;
    }
    
};

std::string brainstorm_cpp(std::string seed, std::string voucher, std::string pack, std::string tag, double souls, bool observatory, bool perkeo, bool copymoney, bool retcon, bool bean, bool burglar, std::string customFilter) {
    BRAINSTORM_PACK = stringToItem(pack);
    BRAINSTORM_TAG = stringToItem(tag);
    BRAINSTORM_VOUCHER = stringToItem(voucher);
	BRAINSTORM_FILTER = stringToFilter(customFilter);
    BRAINSTORM_SOULS = souls;
    BRAINSTORM_OBSERVATORY = observatory;
    BRAINSTORM_PERKEO = perkeo;
	BRAINSTORM_EARLYCOPY = copymoney;
	BRAINSTORM_RETCON = retcon;
	BRAINSTORM_ANTE6_BURGLAR = burglar;
	BRAINSTORM_ANTE8_BEAN = bean;
    Search search(filter, seed, 12, 2318107019761);
    search.exitOnFind = true;
    return search.search();
}

extern "C" {
    const char* brainstorm(const char* seed, const char* voucher, const char* pack, const char* tag, double souls, bool observatory, bool perkeo, bool copymoney, bool retcon, bool bean, bool burglar, const char* customFilter) {
        std::string cpp_seed(seed);
        std::string cpp_pack(pack);
        std::string cpp_voucher(voucher);
        std::string cpp_tag(tag);
        std::string cpp_filter(customFilter);
        std::string result = brainstorm_cpp(cpp_seed, cpp_voucher, cpp_pack, cpp_tag, souls, observatory, perkeo, copymoney, retcon, bean, burglar, cpp_filter);

        char* c_result = (char*)malloc(result.length() + 1);
        strcpy(c_result, result.c_str());

        return c_result;
    }

    void free_result(const char* result) {
        free((void*)result);
    }
}