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
  long legendaries = 0;
  inst.nextPack(1);
  for (int p = 1; p <= 3; p++) {
    Pack pack = packInfo(inst.nextPack(1));
    if (pack.type == Item::Arcana_Pack) {
      auto packContents = inst.nextArcanaPack(pack.size, 1);
      for (int x = 0; x < pack.size; x++) {
        if (packContents[x] == Item::The_Soul)
          legendaries++;
      }
    }
    if (pack.type == Item::Spectral_Pack) {
      auto packContents = inst.nextSpectralPack(pack.size, 1);
      for (int x = 0; x < pack.size; x++) {
        if (packContents[x] == Item::The_Soul)
          legendaries++;
      }
    }
  }
  return legendaries;
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
  Search search(filter_retcon, "11111J31", 12, 2318107019761);
  search.highScore = 1;
  search.printDelay = 100000;
  search.search();
  return 1;
}