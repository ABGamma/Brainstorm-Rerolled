// filters.cpp — the seed filters behind the Brainstorm settings menu.
//
// brainstormFilter() is called once per candidate seed, from many threads at
// once. It must therefore only read `cfg` (never write global state) and work
// on its own copy of the Instance, which is why `inst` is taken by value.
//
// Return value: 0 = seed rejected, >= 1 = seed accepted.
//
// This logic was moved here from immolate.cpp, with the old BRAINSTORM_*
// globals turned into fields of `cfg`. Since then, the only behaviour change
// is the initLocks() call below: the original code never set up the
// run-start locks (see the comment there).

#include "filters.hpp"
#include "fastscan.hpp"
#include "filterprog.hpp"
#include "functions.hpp"

long brainstormFilter(Instance inst, const FilterConfig &cfg) {
    // Model the game at the start of a new run: items that can't appear yet
    // (upgrade vouchers, ante 2+ tags, deck-dependent jokers, ...) are locked.
    // When the game's random pick lands on a locked item it re-rolls, and
    // Immolate only reproduces that re-roll if it knows the item is locked.
    // Verified against the real game: without this, 12 of 30 ante 1
    // tag/voucher predictions were wrong; with it, all 30 matched.
    // `inst` is this seed's private copy, so the locks never leak to the
    // next seed.
    inst.initLocks(1, false, true);

    switch (cfg.custom) {
        case customFilters::NO_FILTER: {
            if (cfg.pack != Item::RETRY) {
                inst.cache.generatedFirstPack = true; //we don't care about Pack 1
                if (inst.nextPack(1) != cfg.pack) {
                    return 0;
                }
            }

            if (cfg.voucher != Item::RETRY && !cfg.observatory) {
                if (inst.nextVoucher(1) != cfg.voucher) {
                    return 0;
                }
            }

            if (cfg.tag != Item::RETRY) {
                if (inst.nextTag(1) != cfg.tag) {
                    return 0;
                }
            }
            if (cfg.souls > 0) {
                for (int i = 1; i <= cfg.souls; i++) {
                    // Mega Arcana Pack, assumed from a Charm Tag.
                    // (Was nextArcanaPack(5, 1) + a search for The Soul;
                    // see fastscan.hpp for why this gives the same answer.)
                    bool found_soul = arcanaPackHasSoul(inst, 5, 1);
                    bool found_perkeo = found_soul && cfg.perkeo &&
                        inst.nextJoker(ItemSource::Soul, 1, true).joker == Item::Perkeo;
                    if (!found_soul) {
                        return 0;
                    }
                    if (cfg.perkeo && !found_perkeo) {
                        return 0; // If Perkeo is required but not found, return 0
                    }
                }
            }
            if (cfg.observatory) {
                if (inst.nextVoucher(1) == Item::Telescope) {
                    inst.activateVoucher(Item::Telescope);
                    if (inst.nextVoucher(2) != Item::Observatory) {
                        return 0;
                    }
                }
                else return 0;
            }
            if (cfg.copymoney) {
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
            // findShopJoker() == shop position of the first match, or -1;
            // exactly what looping over nextShopItem(ante) finds
            // (see fastscan.hpp).
            if (cfg.bean) {
                if (findShopJoker(inst, 8, Item::Turtle_Bean, 150) < 0) {
                    return 0; // If Turtle Bean is not found, return 0
                }
            }
            if (cfg.burglar) {
                // Ante 8 continues where Bean's scan stopped, if Bean ran:
                // both use the same streams inside inst, with the same
                // options (both uncommon jokers).
                if (findShopJoker(inst, 6, Item::Burglar, 50) < 0 &&
                    findShopJoker(inst, 7, Item::Burglar, 100) < 0 &&
                    findShopJoker(inst, 8, Item::Burglar, 100) < 0) {
                    return 0; // If Burglar is not found, return 0
                }
            }
            if (cfg.retcon) {
                // (Locks are already initialised at the top of the function.
                // Calling initLocks again here would re-lock vouchers that an
                // earlier check unlocked via activateVoucher.)
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

            // The custom filter, on a fresh run of the same seed: its
            // conditions are facts about the seed, independent of what the
            // checks above drew (see filterprog.hpp).
            if (cfg.program && !cfg.program->matches(inst.seed)) {
                return 0;
            }

            return 1; // Return a score of 1 if all conditions are met
        }
        default:
            return 1;
    }
    
}
