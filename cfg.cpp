#include "fatcfg.h"
#include "cfg.h"

RTC_DATA_ATTR char CFG_WIFI_NAME[33];
RTC_DATA_ATTR char CFG_WIFI_PASS[65];
RTC_DATA_ATTR char CFG_NTP_SERVER[256];
RTC_DATA_ATTR char CFG_POSIX_TZ[256];
RTC_DATA_ATTR bool CFG_24_HOUR = false;
RTC_DATA_ATTR uint32_t CFG_LIGHT1_COLOR = 0xFF0000;
RTC_DATA_ATTR uint32_t CFG_LIGHT2_COLOR = 0xFFFFFF;
RTC_DATA_ATTR uint16_t CFG_LIGHT1_BRIGHTNESS = 50;
RTC_DATA_ATTR uint16_t CFG_LIGHT2_BRIGHTNESS = 50;
RTC_DATA_ATTR uint64_t CFG_LIGHT1_TIMEOUT_US = 30*1000000L;
RTC_DATA_ATTR uint64_t CFG_LIGHT2_TIMEOUT_US = 20*1000000L;

void cfg_init() {
  fatcfg_get_string(CFG_WIFI_NAME, 33, "WiFiName.txt", "MyWiFiName");
  fatcfg_get_string(CFG_WIFI_PASS, 65, "WiFiPass.txt", "MyWifiPass");
  fatcfg_get_string(CFG_NTP_SERVER, 256, "NTPServer.txt", "pool.ntp.org");
  fatcfg_get_string(CFG_POSIX_TZ, 256, "TimezonePOSIX.txt", "MST7MDT,M3.2.0,M11.1.0");
  CFG_24_HOUR           = fatcfg_get_bool("24HourClock.txt", "Yes");
  CFG_LIGHT1_COLOR      = fatcfg_get_num("Light1Color.txt", "0xFF0000");
  CFG_LIGHT2_COLOR      = fatcfg_get_num("Light2Color.txt", "0xFFFFFF");
  CFG_LIGHT1_BRIGHTNESS = fatcfg_get_num("Light1Brightness.txt", "50");
  CFG_LIGHT2_BRIGHTNESS = fatcfg_get_num("Light2Brightness.txt", "100");
  CFG_LIGHT1_TIMEOUT_US = 1000000L*fatcfg_get_num("Light1Timeout.txt", "15");
  CFG_LIGHT2_TIMEOUT_US = 1000000L*fatcfg_get_num("Light2Timeout.txt", "30");
}

