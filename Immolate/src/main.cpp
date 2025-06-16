#include "functions.cpp"
#include "functions.hpp"
#include "instance.hpp"
#include "items.cpp"
#include "items.hpp"
#include "rng.cpp"
#include "rng.hpp"
#include "search.hpp"
#include "search.cpp"
#include "seed.cpp"
#include "seed.hpp"
#include "util.cpp"
#include "util.hpp"
#include <iomanip>
#include <iostream>
#include <vector>
#include <chrono>

long filter(Instance inst) {
    bool negativePerkeo = false;
    inst.initLocks(1, false, true);
    bool observatory = false;
    bool telescope = false;
    
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
    for (int i = 1; i < 9; i++) {
        auto voucher = inst.nextVoucher(i);
        inst.activateVoucher(voucher);
        if (voucher == Item::Telescope || telescope) {
            telescope = true;
            if (voucher == Item::Observatory) {
                observatory = true;
            }
        }
    }
    if (!observatory) {
        return 0; // If Retcon is not found, return 0
    }
    bool bprint = false;
    for (int i = 0; i < 2; i++) {
        ShopItem item = inst.nextShopItem(1);
        if (item.type == Item::Joker) {
            if (item.jokerData.joker == Item::Blueprint) {
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
                if (packContents[x].joker == Item::Blueprint) {
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
};


//Currently filtering for bosses does not seem to work
long filter_crimson_heart(Instance inst) {
    inst.initLocks(1, false, true);
	printf("Checking for Crimson Heart...\n");
	inst.nextBoss(1);
    printf("Boss: %s\n");
    for (int i = 2; i < 8; i++) {
        if (i < 7) {
            //inst.initUnlocks(i, false);
        }
        inst.nextBoss(i);
        printf("Boss: %s\n");
    }
	
	if(inst.nextBoss(8) != Item::Crimson_Heart) {
        return 0;
    }
	return 1;
}

long filter_retcon(Instance inst) {
    inst.initLocks(1, false, true);
	bool directorsCut = false;
    for(int i = 1; i < 9; i++) {
        auto voucher = inst.nextVoucher(i);
		inst.activateVoucher(voucher);
        if (voucher == Item::Directors_Cut || directorsCut) {
            directorsCut = true;
            if (voucher == Item::Retcon) {
                return 1;
            }
        }
	}

    return 0;
}

long filter_blank(Instance inst) { return 0; }


int main() {
  //benchmark_single();
  //benchmark_quick();
  //benchmark_quick_lucky();
  //benchmark_blank();
  //benchmark();
  Search search(filter, "KY9AZX31", 12, 2318107019761);
  search.highScore = 1;
  search.printDelay = 100000;
  search.exitOnFind = true;
  search.search();
  return 1;
}