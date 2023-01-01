#ifndef _CFG_H_
#define _CFG_H_

#include <driver/rtc_io.h>

const uint64_t CFG_WIFI_TIMEOUT_US = 60000000UL; // 1 minutes to connect and sync

extern RTC_DATA_ATTR char CFG_WIFI_NAME[33];
extern RTC_DATA_ATTR char CFG_WIFI_PASS[65];
extern RTC_DATA_ATTR char CFG_NTP_SERVER[256];
extern RTC_DATA_ATTR char CFG_POSIX_TZ[256];
extern RTC_DATA_ATTR bool CFG_24_HOUR;
extern RTC_DATA_ATTR uint32_t CFG_LIGHT1_COLOR;
extern RTC_DATA_ATTR uint32_t CFG_LIGHT2_COLOR;
extern RTC_DATA_ATTR uint16_t CFG_LIGHT1_BRIGHTNESS;
extern RTC_DATA_ATTR uint16_t CFG_LIGHT2_BRIGHTNESS;
extern RTC_DATA_ATTR uint64_t CFG_LIGHT1_TIMEOUT_US;
extern RTC_DATA_ATTR uint64_t CFG_LIGHT2_TIMEOUT_US;

void cfg_init();

#endif
