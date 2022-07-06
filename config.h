// TODO: make configurable in nonvolatile memory
static const char* const WIFI_SSID     = "wifi-name";
static const char* const WIFI_PASSWORD = "wifi-password";
static const uint64_t WIFI_TIMEOUT_US = 30*1000000L;
static const char* const NTP_SERVER1   = "pool.ntp.org";
static const char* const NTP_SERVER2   = "pool.ntp.org";
static const char* const NTP_SERVER3   = "pool.ntp.org";
static const char* const POSIX_TX      = "MST7MDT,M3.2.0,M11.1.0"; // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
static const uint8_t HOUR_12_24_N = 1;
static const uint32_t LIGHT1_COLOR = 0xFF0000;
static const uint16_t LIGHT1_BRIGHTNESS = 50;
static const uint64_t LIGHT1_TIMEOUT_US = 30*1000000L;
static const uint32_t LIGHT2_COLOR = 0xFFFFFF;
static const uint16_t LIGHT2_BRIGHTNESS = 50;
static const uint64_t LIGHT2_TIMEOUT_US = 20*1000000L;
