#ifndef _FATCFG_H_
#define _FATCFG_H_

#include <stdint.h>

void fatcfg_init();

void fatcfg_msc_init();

void fatcfg_get_string(char* buf, int maxchars, const char* fname, const char* fallback);

bool fatcfg_get_bool(const char* fname, const char* fallback);

uint64_t fatcfg_get_num(const char* fname, const char* fallback);

#endif
