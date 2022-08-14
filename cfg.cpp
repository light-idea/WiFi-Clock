#include "fatcfg.h"
#include "cfg.h"

// Only need to be read on startup.
char CFG_WIFI_NAME[33];
char CFG_WIFI_PASS[65];
char CFG_NTP_SERVER[256];
char CFG_POSIX_TZ[256];
RTC_DATA_ATTR bool CFG_24_HOUR = false;
RTC_DATA_ATTR uint32_t CFG_LIGHT1_COLOR = 0xFF0000;
RTC_DATA_ATTR uint32_t CFG_LIGHT2_COLOR = 0xFFFFFF;
RTC_DATA_ATTR uint16_t CFG_LIGHT1_BRIGHTNESS = 50;
RTC_DATA_ATTR uint16_t CFG_LIGHT2_BRIGHTNESS = 50;
RTC_DATA_ATTR uint64_t CFG_LIGHT1_TIMEOUT_US = 30*1000000L;
RTC_DATA_ATTR uint64_t CFG_LIGHT2_TIMEOUT_US = 20*1000000L;

void cfg_init() {
  fatcfg_get_string(CFG_WIFI_NAME, "WiFiName.txt", "MyWiFiName");
  fatcfg_get_string(CFG_WIFI_PASS, "WiFiPass.txt", "MyWifiPass");
  fatcfg_get_string(CFG_NTP_SERVER, "NTPServer.txt", "pool.ntp.org");
  fatcfg_get_string(CFG_POSIX_TZ, "POSIX_Timezone.txt", "MST7MDT,M3.2.0,M11.1.0");
}

