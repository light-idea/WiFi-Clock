#include <driver/rtc_io.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <time.h>

//#include <GxEPD2_4G_4G.h>
//#include <GxEPD2_4G_BW.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>

#include "fonts/NotoSerif_Regular10pt7b.h"
#include "fonts/NotoSerif_Regular56pt7b.h"

// Peripherals
//GxEPD2_4G_4G<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5
GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(4, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define DISPLAY_W 296
#define DISPLAY_H 128
#define DISPLAY_ROTATION 3

#define PADDING_TOP 1
#define PADDING_BOT 11
#define TIME_OFFSET_X -6
#define DATE_OFFSET_X -5

#define PM_WIDTH 12
#define PM_HEIGHT 12

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

void sync_time() {
  WiFi.mode(WIFI_STA);
  //WiFi.config(IP, GATEWAY, SUBNET, DNS); // Uncomment to use static addresses
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTzTime(POSIX_TX, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
}

void disp_update(char* date, char* time, uint8_t full_update) {
  int16_t x, y;
  uint16_t w, h;
  int16_t date_x, date_y, time_x, time_y;
  display.setFont(&NotoSerif_Regular10pt7b[0]);
  display.getTextBounds(date, 0, 0, &x, &y, &w, &h); //calc width of new string
  date_x = (DISPLAY_W/2 - w/2) + DATE_OFFSET_X;
  date_y = h + PADDING_TOP;
  display.setFont(&NotoSerif_Regular56pt7b[0]);
  display.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  Serial.print("Full update: ");
  Serial.println(full_update);

  //if (full_update) {
  //  display.setFullWindow();
  //}
  //else {
    display.setPartialWindow(0, 0, DISPLAY_W, DISPLAY_H);
  //}
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    // Date
    for (int i = 2; i >= 0; --i) {
      display.setTextColor((i==2) ? GxEPD_LIGHTGREY : (i==1) ? GxEPD_DARKGREY : GxEPD_BLACK);
      display.setFont(&NotoSerif_Regular10pt7b[i]);
      display.setCursor(date_x, date_y);
      display.print(date);
    }
    // Time
    for (int i = 2; i >= 0; --i) {
      display.setTextColor((i==2) ? GxEPD_LIGHTGREY : (i==1) ? GxEPD_DARKGREY : GxEPD_BLACK);
      display.setFont(&NotoSerif_Regular56pt7b[i]);
      display.setCursor(time_x, time_y);
      display.print(time);
    }
    if (HOUR_12_24_N) {
      display.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                            PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display.nextPage());
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
    first_update = 1;
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
  strftime(str_date, sizeof(str_date), "%A, %b %e %G", &now);
  if (HOUR_12_24_N) {
    strftime(str_time, sizeof(str_time), "%l:%M", &now);
  }
  else {
    strftime(str_time, sizeof(str_time), "%I:%M", &now);
  }

  Serial.print("firstupdate: ");
  Serial.println(first_update);

  if (now.tm_min != last_tm_min) {
    display.init(115200);
    display.setRotation(3);
    disp_update(str_date, str_time, (now.tm_min % 5)==0 || first_update);
    display.hibernate();

    first_update = 0;
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
    display.powerOff();
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
