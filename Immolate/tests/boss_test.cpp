// boss_test.cpp — checks Instance::nextBoss() (src/functions.hpp).
//
// 1. Seeds whose bosses for antes 1..8 were read from the real game
//    (October 2026: 32 seeds, all 256 bosses agreed; a few of them are
//    copied here).
// 2. The game's rules on many seeds: no boss repeats within antes 1..7,
//    every boss has reached its minimum ante, ante 8 is a showdown boss.

#include "functions.hpp"

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool ok, const std::string &what) {
  if (!ok) {
    failures++;
    std::printf("FAIL %s\n", what.c_str());
  }
}

static std::string bosses(const char *seed) {
  Seed s(seed);
  Instance inst(s);
  std::string out = seed;
  for (int ante = 1; ante <= 8; ante++)
    out += "|" + itemToString(inst.nextBoss(ante));
  return out;
}

int main() {
  // As the game picked them: the ante 1 boss of a fresh run, then the
  // game's get_new_boss() for antes 2..8.
  const char *fromGame[] = {
      "AAAAAAAA|The Manacle|The House|The Fish|The Window|The Wall|The Tooth|The Flint|Verdant Leaf",
      "M4Q2ZZ71|The Pillar|The Needle|The Head|The Plant|The Fish|The Arm|The Mark|Violet Vessel",
      "VJ1DEGYD|The Psychic|The Club|The Tooth|The Water|The Hook|The Wall|The Arm|Amber Acorn",
      "F3DHPD1D|The Window|The Pillar|The Wall|The Plant|The Arm|The Manacle|The Needle|Crimson Heart",
      "73V55YUQ|The Goad|The Club|The Arm|The Eye|The Mark|The Tooth|The Window|Cerulean Bell",
      "N9YJB9UF|The Goad|The Flint|The Mark|The Window|The Mouth|The Psychic|The Ox|Cerulean Bell",
      "R9YKXP8W|The Club|The Window|The Hook|The Flint|The Manacle|The Plant|The Serpent|Crimson Heart",
      "4XYFPGP6|The Psychic|The Mouth|The Water|The Tooth|The Goad|The Ox|The Needle|Verdant Leaf",
  };
  for (const char *expected : fromGame) {
    std::string seed = std::string(expected).substr(0, 8);
    std::string got = bosses(seed.c_str());
    check(got == expected, "game seed\n  want " + std::string(expected) +
                               "\n  got  " + got);
  }

  // Minimum ante per boss, from the game's G.P_BLINDS (0 = showdown).
  auto minAnte = [](Item b) {
    switch (b) {
    case Item::The_Club: case Item::The_Goad: case Item::The_Head:
    case Item::The_Hook: case Item::The_Manacle: case Item::The_Pillar:
    case Item::The_Psychic: case Item::The_Window:
      return 1;
    case Item::The_Eye: case Item::The_Tooth: return 3;
    case Item::The_Plant: return 4;
    case Item::The_Serpent: return 5;
    case Item::The_Ox: return 6;
    default:
      return b > Item::B_F_BEGIN ? 0 : 2;
    }
  };

  const char chars[] = "123456789ABCDEFGHIJKLMNPQRSTUVWXYZ";
  int seeds = 0;
  for (int i = 0; i < 20000; i++) {
    char name[9];
    unsigned v = 2654435761u * (unsigned)(i + 1);
    for (int c = 0; c < 8; c++) {
      name[c] = chars[v % 34];
      v = v / 34 + 2654435761u * (unsigned)c;
    }
    name[8] = 0;
    Seed s(name);
    Instance inst(s);
    bool seen[(int)Item::ITEMS_END] = {false};
    for (int ante = 1; ante <= 8; ante++) {
      Item b = inst.nextBoss(ante);
      int m = minAnte(b);
      std::string where = std::string(name) + " ante " + std::to_string(ante) +
                          ": " + itemToString(b);
      if (ante == 8) {
        check(m == 0, where + " is not a showdown boss");
      } else {
        check(m != 0 && m <= ante, where + " not allowed yet");
        check(!seen[(int)b], where + " repeated");
      }
      seen[(int)b] = true;
    }
    seeds++;
  }

  std::printf("%d game seeds, %d rule seeds: %s\n",
              (int)(sizeof(fromGame) / sizeof(*fromGame)), seeds,
              failures ? "FAILED" : "ok");
  return failures ? 1 : 0;
}
