// immolate.cpp — legacy blocking brainstorm() export, implemented on top of
// the asynchronous API. See immolate.hpp for why it still exists.
//
// The filter logic that used to live here is now in filters.cpp.

#include "immolate.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {

BS_API const char *brainstorm(const char *seed, const char *voucher,
                              const char *pack, const char *tag, double souls,
                              bool observatory, bool perkeo, bool copymoney,
                              bool retcon, bool bean, bool burglar,
                              const char *customFilter) {
  bs_params p = {};
  p.api_version = BS_API_VERSION;
  p.start_seed = seed;
  p.threads = 0;
  p.tag = tag;
  p.voucher = voucher;
  p.pack = pack;
  p.souls = (int32_t)souls;
  p.observatory = observatory;
  p.perkeo = perkeo;
  p.copy_money = copymoney;
  p.retcon = retcon;
  p.bean = bean;
  p.burglar = burglar;
  p.custom_filter = customFilter;

  char found[9] = {0};
  bs_job *job = bs_search_start(&p);
  if (job) {
    while (bs_search_poll(job, found, nullptr) == BS_RUNNING)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    bs_search_free(job);
  }

  char *out = (char *)std::malloc(std::strlen(found) + 1);
  std::strcpy(out, found);
  return out;
}

BS_API void free_result(const char *result) { std::free((void *)result); }

} // extern "C"
