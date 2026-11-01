// immolate.hpp — the ORIGINAL blocking entry point, kept so older copies of
// Brainstorm.lua (or anyone's custom scripts) keep working.
//
// New code should use the asynchronous API in brainstorm_api.h instead:
// this function blocks the calling thread until a seed is found, which is
// what used to freeze the game.
#ifndef IMMOLATE_HPP
#define IMMOLATE_HPP

#include "brainstorm_api.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Item arguments take either game keys or English names; see bs_params.
// Returns a malloc'd seed string ("" if none found); release it with
// free_result().
BS_API const char *brainstorm(const char *seed, const char *voucher,
                              const char *pack, const char *tag, double souls,
                              bool observatory, bool perkeo, bool copymoney,
                              bool retcon, bool bean, bool burglar,
                              const char *customFilter);
BS_API void free_result(const char *result);

#ifdef __cplusplus
}
#endif

#endif // IMMOLATE_HPP
