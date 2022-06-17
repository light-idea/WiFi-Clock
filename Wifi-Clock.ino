#include <driver/rtc_io.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <time.h>

#include <GxEPD2_4G_4G.h>
#include <GxEPD2_4G_BW.h>

#include "fonts/NotoSerif_Regular10pt7b.h"
#include "fonts/NotoSerif_Regular56pt7b.h"

// Peripherals
GxEPD2_4G_4G<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(4, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define DISPLAY_W 296
#define DISPLAY_H 128
#define DISPLAY_ROTATION 3

// TODO: make configurable in nonvolatile memory
static const char* const WIFI_SSID     = "wifi-name";
static const char* const WIFI_PASSWORD = "wifi-password";
static const uint64_t WIFI_TIMEOUT_US = 30*1000000L;
static const char* const NTP_SERVER1   = "pool.ntp.org";
static const char* const NTP_SERVER2   = "pool.ntp.org";
static const char* const NTP_SERVER3   = "pool.ntp.org";
static const char* const POSIX_TX      = "MST7MDT,M3.2.0,M11.1.0"; // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
static const uint32_t LIGHT1_COLOR = 0xFF0000;
static const uint16_t LIGHT1_BRIGHTNESS = 50;
static const uint64_t LIGHT1_TIMEOUT_US = 30*1000000L;
static const uint32_t LIGHT2_COLOR = 0xFFFFFF;
static const uint16_t LIGHT2_BRIGHTNESS = 50;
static const uint64_t LIGHT2_TIMEOUT_US = 20*1000000L;

// Using uint64_t since time_t is small and signed
RTC_DATA_ATTR static uint64_t evt_us_time_sync_timeout = 0;
RTC_DATA_ATTR static uint64_t evt_us_light_off = 0;
RTC_DATA_ATTR static uint64_t evt_us_debounce = 0;

#define DEBOUNCE_STAGES 5
#define DEBOUNCE_STAGE_US 10000
static uint8_t btns[DEBOUNCE_STAGES+1] = {0};
static uint8_t btn;

void sync_time() {
  WiFi.mode(WIFI_STA);
  //WiFi.config(IP, GATEWAY, SUBNET, DNS); // Uncomment to use static addresses
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTzTime(POSIX_TX, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
}

void disp_full_update(char* date, char* time) {
  int16_t x, y;
  uint16_t w, h;
  int16_t dx, dy, tx, ty;
  display.getTextBounds(date, 0, 0, &x, &y, &w, &h); //calc width of new string
  dx = (DISPLAY_W/2 - w) / 2;
  dy = y + 3;
  display.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  tx = (DISPLAY_W/2 - w) / 2;
  ty = dy + y + 3;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    // Date
    for (int i = 2; i >= 0; --i) {
      display.setTextColor((i==2) ? GxEPD_LIGHTGREY : (i==1) ? GxEPD_DARKGREY : GxEPD_BLACK);
      display.setFont(&NotoSerif_Regular10pt7b[i]);
      display.setCursor(dx, dy);
      display.print(date);
    }
    // Time
    for (int i = 2; i >= 0; --i) {
      display.setTextColor((i==2) ? GxEPD_LIGHTGREY : (i==1) ? GxEPD_DARKGREY : GxEPD_BLACK);
      display.setFont(&NotoSerif_Regular56pt7b[i]);
      display.setCursor(tx, ty);
      display.print(time);
    }
  } while (display.nextPage());
}

void disp_partial_update() {
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

  if (wakeup_reason==ESP_SLEEP_WAKEUP_UNDEFINED) { // Not a Wake-up
    // Sync Time over WiFi
    sync_time();
    evt_us_time_sync_timeout = WIFI_TIMEOUT_US;
    // Print Message
    display.init(115200);
    display.setRotation(DISPLAY_ROTATION);
    display.clearScreen();
    display.setPartialWindow(0, DISPLAY_H/2, DISPLAY_W, DISPLAY_H/2);
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(5, DISPLAY_H/2+5);
        display.print("Syncing Time...");
    } while (display.nextPage());
    display.hibernate();
  }

}

void loop() {
  struct timeval tv_now;
  uint64_t epoch_us;
  struct tm now; // Display Time
  static int last_tm_min;
  char str_date[24]; // Longest date string: "Wednesday, Sep 20, 2019" = 23 chars + \0
  char str_time[6]; // Longest time "12:00" = 5 chars + \0


  // Placeholder
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
    pixels.setBrightness(LIGHT1_BRIGHTNESS);
    pixels.fill(LIGHT1_COLOR);
    pixels.show();
    evt_us_light_off = epoch_us + LIGHT1_TIMEOUT_US;
  }

  if (btn == 0xC) {
    display.init(115200);
    display.clearScreen();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
    esp_sleep_enable_timer_wakeup(60*1000000L);
    esp_deep_sleep_start();
  }

  if (btn == 0x1) {
    Serial.println("Turning on lights.");
    digitalWrite(NEOPIXEL_POWER, LOW); // on
    pixels.begin();
    pixels.setBrightness(LIGHT2_BRIGHTNESS);
    pixels.fill(LIGHT2_COLOR);
    pixels.show();
    evt_us_light_off = epoch_us + LIGHT2_TIMEOUT_US;
  }

  if (btn == 0x3) {
    Serial.println("Syncing time.");
    sync_time();
    evt_us_time_sync_timeout = epoch_us + WIFI_TIMEOUT_US;
  }

  /* Update Time */

  getLocalTime(&now, 0);
  strftime(str_date, sizeof(str_date), "%A, $b %e %G", &now);
  strftime(str_time, sizeof(str_time), "%I:%M", &now);

  if (now.tm_min != last_tm_min) {
    display.init(115200);
    display.setRotation(3);
    disp_full_update(str_date, str_time);
    //disp_partial_update();
    display.hibernate();

    last_tm_min = now.tm_min;
  }

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

  //uint64_t delay_ms = (evt_next - epoch_us) / 1000;
  //uint64_t delay_us = (evt_next - epoch_us) % 1000;
  //delay(delay_ms);
  //delayMicroseconds(delay_us);

  uint64_t sleep_us = evt_next - epoch_us;
  Serial.println("Delay us: ");
  Serial.print(sleep_us);

  Serial.flush();

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
    display.powerOff();
    digitalWrite(EPD_RESET, LOW); // hardware power down mode
    esp_deep_sleep_start();
  }
}
