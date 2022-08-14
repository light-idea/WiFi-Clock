#ifndef _FATCFG_H_
#define _FATCFG_H_

void fatcfg_init();

void fatcfg_msc_init();

bool fatcfg_pc_connected();

void fatcfg_get_string(char* buf, const char* fname, const char* fallback);

#endif
