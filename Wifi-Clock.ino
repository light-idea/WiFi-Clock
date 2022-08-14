#include <driver/rtc_io.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <time.h>

#include "cfg.h"
#include "fatcfg.h"

#include "display.h"

// Peripherals
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(4, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Using uint64_t since time_t is small and signed
RTC_DATA_ATTR static uint64_t evt_us_time_sync_timeout = 0;
RTC_DATA_ATTR static uint64_t evt_us_light_off = 0;
RTC_DATA_ATTR static uint64_t evt_us_debounce = 0;
RTC_DATA_ATTR static uint8_t first_update = 0;
RTC_DATA_ATTR static int last_tm_min;

#define DEBOUNCE_STAGES 5
#define DEBOUNCE_STAGE_US 10000
static uint8_t btns[DEBOUNCE_STAGES+1] = {0};
static uint8_t btn;

#define YEAR_2000_US (946684800000L*1000L)

/* Initialize CFG_WIFI_NAME, etc. before calling this function. */
void sync_time() {
  WiFi.mode(WIFI_STA);
  //WiFi.config(IP, GATEWAY, SUBNET, DNS); // Uncomment to use static addresses
  WiFi.begin(CFG_WIFI_NAME, CFG_WIFI_PASS);
  configTzTime(CFG_POSIX_TZ, CFG_NTP_SERVER);
}

void setup() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  pinMode(NEOPIXEL_POWER, OUTPUT);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);
  rtc_gpio_pullup_en(GPIO_NUM_15); // Button A
  rtc_gpio_pullup_en(GPIO_NUM_11); // Button D

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) { // Button Press
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1: 
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
      esp_sleep_enable_timer_wakeup(5000L); // Debounce
      esp_deep_sleep_start();
      break;
  }

  display_init();

  if (wakeup_reason==ESP_SLEEP_WAKEUP_UNDEFINED) { // Not a Wake-up
    fatcfg_init();
    if (fatcfg_pc_connected()) {
      // Configure
      fatcfg_msc_init();
      display_update_all("Configuration mode...", "");
      while(1) { 
        delay(1000); 
        if (digitalRead(BUTTON_A)) { break; }
      } // TODO: button press to escape.
    }
    first_update = 1;
    display_update_all("Syncing time...", "");
    // Sync Time over WiFi
    cfg_init();
    sync_time();
    evt_us_time_sync_timeout = CFG_WIFI_TIMEOUT_US;
  }

}

void loop() {
  struct timeval tv_now;
  uint64_t epoch_us;
  struct tm now; // Display Time
  char str_date[24]; // Longest date string: "Wednesday, Sep 20, 2019" = 23 chars + \0
  char str_time[6]; // Longest time "12:00" = 5 chars + \0

  btn = 0;
  if (!digitalRead(BUTTON_A)) { btn |= 0x8; }
  if (!digitalRead(BUTTON_B)) { btn |= 0x4; }
  if (!digitalRead(BUTTON_C)) { btn |= 0x2; }
  if (!digitalRead(BUTTON_D)) { btn |= 0x1; }
  Serial.print("Buttons: ");
  Serial.println(btn);

  gettimeofday(&tv_now, NULL);
  epoch_us = (uint64_t)tv_now.tv_sec*1000000L + (uint64_t)tv_now.tv_usec;

  /* Event Start */

  if (btn == 0x8) {
    Serial.println("Turning on lights.");
    digitalWrite(NEOPIXEL_POWER, LOW); // on
    pixels.begin();
    pixels.setBrightness(CFG_LIGHT1_BRIGHTNESS);
    pixels.fill(CFG_LIGHT1_COLOR);
    pixels.show();
    evt_us_light_off = epoch_us + CFG_LIGHT1_TIMEOUT_US;
  }

  if (btn == 0xC) {
    display_clear();
    first_update = 1;
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
    esp_sleep_enable_timer_wakeup(60*1000000L);
    esp_deep_sleep_start();
  }

  if (btn == 0x1) {
    Serial.println("Turning on lights.");
    digitalWrite(NEOPIXEL_POWER, LOW); // on
    pixels.begin();
    pixels.setBrightness(CFG_LIGHT2_BRIGHTNESS);
    pixels.fill(CFG_LIGHT2_COLOR);
    pixels.show();
    evt_us_light_off = epoch_us + CFG_LIGHT2_TIMEOUT_US;
  }

  if (btn == 0x3) {
    Serial.println("Syncing time.");
    sync_time();
    evt_us_time_sync_timeout = epoch_us + CFG_WIFI_TIMEOUT_US;
  }

  /* Update Time */

  getLocalTime(&now, 0);
  strftime(str_date, sizeof(str_date), "%A, %b %e %G", &now);
  if (CFG_24_HOUR) {
    strftime(str_time, sizeof(str_time), "%I:%M", &now);
  }
  else {
    strftime(str_time, sizeof(str_time), "%l:%M", &now);
  }

  Serial.print("firstupdate: ");
  Serial.println(first_update);

  //if (epoch_us < YEAR_2000_US) {
  //  display_update_error("Unable to sync time!", "");
  //} 
  //else {
    if (now.tm_min != last_tm_min) {
      if (now.tm_min % 5 == 0 || first_update) {
        display_update_all(str_date, str_time);
      }
      else {
        display_update_time(str_time);
      }

      first_update = 0;
      last_tm_min = now.tm_min;
    }
  //}

  gettimeofday(&tv_now, NULL);
  epoch_us = (uint64_t)tv_now.tv_sec*1000000L + (uint64_t)tv_now.tv_usec;

  /* Event End */

  if (epoch_us > evt_us_light_off) {
    Serial.println("Turning off lights");
    pixels.clear();
    digitalWrite(NEOPIXEL_POWER, HIGH); // off
  }

  if (epoch_us > evt_us_time_sync_timeout) {
    Serial.println("Turning off WiFi");
    WiFi.mode(WIFI_OFF);
  }

  /* Sleep */

  uint64_t evt_next = epoch_us + (60*1000000L) - (epoch_us % (60*1000000L)); // next display update
  if (evt_us_light_off         > epoch_us && evt_next > evt_us_light_off)         evt_next = evt_us_light_off;
  if (evt_us_time_sync_timeout > epoch_us && evt_next > evt_us_time_sync_timeout) evt_next = evt_us_time_sync_timeout;


  uint64_t sleep_us = evt_next - epoch_us;
  Serial.println("Delay us: ");
  Serial.print(sleep_us);

  Serial.flush();

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
