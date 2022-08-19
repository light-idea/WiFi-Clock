#include <driver/rtc_io.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <time.h>

#include "cfg.h"
#include "fatcfg.h"

#include "display.h"

#define DEBOUNCE_US 5000L
#define YEAR_2000_US (946684800000L*1000L)

/* Peripherals */
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(4, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

/* Flags */
#define FLAG_SYNCING       0x01
#define FLAG_ERR_WIFI_CONN 0x02
#define FLAG_ERR_TIME_SYNC 0x04
RTC_DATA_ATTR static uint8_t flags;

/* Events */
// Using uint64_t since time_t is small and signed
RTC_DATA_ATTR static uint64_t evt_us_time_sync_timeout = 0;
RTC_DATA_ATTR static uint64_t evt_us_light_off = 0;
RTC_DATA_ATTR static uint64_t evt_us_disp_redraw = 0;

/* Display */
RTC_DATA_ATTR static int last_min = INT_MIN;
RTC_DATA_ATTR static int last_day = INT_MIN;

/* Initialize CFG_WIFI_NAME, etc. before calling this function. */
void sync_time() {
  WiFi.mode(WIFI_STA);
  //WiFi.config(IP, GATEWAY, SUBNET, DNS); // Uncomment to use static addresses
  WiFi.begin(CFG_WIFI_NAME, CFG_WIFI_PASS);
  configTzTime(CFG_POSIX_TZ, CFG_NTP_SERVER);
}

uint8_t btn_read() {
  uint8_t btn = 0;
  if (!digitalRead(BUTTON_A)) { btn |= 0x8; }
  if (!digitalRead(BUTTON_B)) { btn |= 0x4; }
  if (!digitalRead(BUTTON_C)) { btn |= 0x2; }
  if (!digitalRead(BUTTON_D)) { btn |= 0x1; }
  return btn;
}

void setup() {
  uint8_t btn;
  esp_sleep_wakeup_cause_t wakeup_reason;

  flags = 0;

  pinMode(NEOPIXEL_POWER, OUTPUT);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);
  rtc_gpio_pullup_en(GPIO_NUM_15); // Button A
  rtc_gpio_pullup_en(GPIO_NUM_11); // Button D

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) { // Button press
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1: 
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
      esp_sleep_enable_timer_wakeup(DEBOUNCE_US); // Debounce
      esp_deep_sleep_start();
      break;
  }

  display_init();

  
  if (wakeup_reason==ESP_SLEEP_WAKEUP_UNDEFINED) { // Startup
    // Configure
    fatcfg_init();
    if (fatcfg_pc_connected()) {
      // Configure
      fatcfg_msc_init();
      display_update_all("Configuration mode...", "", false);
      while(1) { 
        btn = btn_read();
        if (btn) {
          delay(DEBOUNCE_US/1000);
          btn = btn_read();
          if (btn) break;
        }
      }
    }
    // Sync Time over WiFi
    cfg_init();
    sync_time();
    flags |= FLAG_SYNCING;
    flags &= ~(FLAG_ERR_WIFI_CONN | FLAG_ERR_TIME_SYNC);
    evt_us_time_sync_timeout = 0 + CFG_WIFI_TIMEOUT_US;
  }

}

void loop() {
  // Buttons
  uint8_t btn;
  // Epoch Time
  struct timeval tv_now;
  uint64_t epoch_us;
  // Display Time
  struct tm now;
  char str_date[24]; // Longest date string: "Wednesday, Sep 20, 2019" = 23 chars + \0
  char str_time[6]; // Longest time "12:00" = 5 chars + \0
  int curr_min;
  int curr_day;
  bool pm;

  /* Event Start */

  btn = btn_read();
  gettimeofday(&tv_now, NULL);
  epoch_us = (uint64_t)tv_now.tv_sec*1000000L + (uint64_t)tv_now.tv_usec;

  if (btn == 0x8) { // Turn on lights
    digitalWrite(NEOPIXEL_POWER, LOW); // on
    pixels.begin();
    pixels.setBrightness(CFG_LIGHT1_BRIGHTNESS);
    pixels.fill(CFG_LIGHT1_COLOR);
    pixels.show();
    evt_us_light_off = epoch_us + CFG_LIGHT1_TIMEOUT_US;
  }
  if (btn == 0xC) { // Clear screen for 1 minute (
    display_clear();
    evt_us_disp_redraw = epoch_us + 60*1000000L;
  }
  if (btn == 0x1) { // Turn on lights
    digitalWrite(NEOPIXEL_POWER, LOW); // on
    pixels.begin();
    pixels.setBrightness(CFG_LIGHT2_BRIGHTNESS);
    pixels.fill(CFG_LIGHT2_COLOR);
    pixels.show();
    evt_us_light_off = epoch_us + CFG_LIGHT2_TIMEOUT_US;
  }
  if (btn == 0x3) {
    cfg_init();
    sync_time();
    flags |= FLAG_SYNCING;
    flags &= ~(FLAG_ERR_WIFI_CONN | FLAG_ERR_TIME_SYNC);
    evt_us_time_sync_timeout = epoch_us + CFG_WIFI_TIMEOUT_US;
  }

  /* Update Time */

  getLocalTime(&now, 0);
  pm = false;
  // Date
  if (flags & FLAG_ERR_WIFI_CONN) { curr_day = -1;
    strcpy(str_date, "No WiFi connection!");
  }
  else if (flags & FLAG_ERR_TIME_SYNC) { curr_day = -2;
    strcpy(str_date, "Unable to sync time!");
  }
  else if (flags & FLAG_SYNCING) { curr_day = -3;
    strcpy(str_date, "Syncing time...");
  }
  else { curr_day = now.tm_yday;
    strftime(str_date, sizeof(str_date), "%A, %b %e %G", &now);
  }
  // Time
  if (epoch_us < YEAR_2000_US) { curr_min = -1;
    strcpy(str_time, "");
  }
  else if (CFG_24_HOUR) { curr_min = now.tm_min;
    strftime(str_time, sizeof(str_time), "%I:%M", &now);
  }
  else { curr_min = now.tm_min;
    strftime(str_time, sizeof(str_time), "%l:%M", &now);
    if (now.tm_hour >= 12) pm = true;
  }

  if (curr_day != last_day || curr_min % 5 == 0) {
    if (flags & (FLAG_ERR_WIFI_CONN | FLAG_ERR_TIME_SYNC)) {
      display_update_error(str_date, str_time, pm);
    }
    else {
      display_update_all(str_date, str_time, pm);
    }
    last_day = curr_day;
    last_min = curr_min;
  }
  else if (curr_min != last_min) {
    display_update_time(str_time, pm);
    last_min = curr_min;
  }

  gettimeofday(&tv_now, NULL);
  epoch_us = (uint64_t)tv_now.tv_sec*1000000L + (uint64_t)tv_now.tv_usec;

  /* Event End */

  if (epoch_us > evt_us_light_off) {
    pixels.clear();
    digitalWrite(NEOPIXEL_POWER, HIGH); // off
  }
  if (epoch_us > evt_us_time_sync_timeout) {
    if (flags & FLAG_SYNCING) {
      if (WiFi.status() != WL_CONNECTED) { flags |= FLAG_ERR_WIFI_CONN; }
      if (epoch_us < YEAR_2000_US)       { flags |= FLAG_ERR_TIME_SYNC; }
    }
    flags &= ~(FLAG_SYNCING);
    WiFi.mode(WIFI_OFF);
  }

  /* Sleep */

  uint64_t evt_next = epoch_us + (60*1000000L) - (epoch_us % (60*1000000L)); // next display update
  if (evt_us_light_off         > epoch_us && evt_next > evt_us_light_off) {
    evt_next = evt_us_light_off;
  }
  if (evt_us_time_sync_timeout > epoch_us && evt_next > evt_us_time_sync_timeout) {
    evt_next = evt_us_time_sync_timeout;
  }
  uint64_t sleep_us = evt_next - epoch_us;

#ifdef SLEEP_ENABLE
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // needed to keep button pullups enabled, NEOPIXEL_POWER HIGH
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0); // Wake on button A press
  esp_sleep_enable_ext1_wakeup(1<<11, ESP_EXT1_WAKEUP_ALL_LOW); // Wake on button D press
  esp_sleep_enable_timer_wakeup(sleep_us); // Enter sleep until next event
  if (epoch_us < evt_us_time_sync_timeout || // waiting for time sync or light off
      epoch_us < evt_us_light_off) {
    esp_light_sleep_start();
  }
  else { // Deep Sleep
    WiFi.mode(WIFI_OFF);
    digitalWrite(NEOPIXEL_POWER, HIGH); // off
    digitalWrite(SPEAKER_SHUTDOWN, LOW); // off
    digitalWrite(EPD_RESET, LOW); // hardware power down mode
    esp_deep_sleep_start();
  }
#else
  uint64_t delay_ms = (evt_next - epoch_us) / 1000;
  uint64_t delay_us = (evt_next - epoch_us) % 1000;
  delay(delay_ms);
  delayMicroseconds(delay_us);
#endif

}
