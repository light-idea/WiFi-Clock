#include <driver/rtc_io.h>
#include <esp_wifi.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <sntp.h>

#include "cfg.h"
#include "fatcfg.h"

#include "display.h"

#undef CLKDBG

#define DEBOUNCE_US (5000UL)
#define YEAR_2000_US (946684800000000UL)
#define RESYNC_US (14400000000UL)
#define WIFI_POWEROFF_US (3000000UL)

/* Peripherals */
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(4, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

/* Flags */
#define FLAG_SYNCING       0x01
#define FLAG_LIGHT1        0x02
#define FLAG_LIGHT2        0x04
#define FLAG_CLEAR_DISP    0x08
#define FLAG_ERR_WIFI_CONN 0x10
#define FLAG_ERR_TIME_SYNC 0x20
RTC_DATA_ATTR static uint8_t flags;

/* Events */
// Using uint64_t since time_t is small and signed
RTC_DATA_ATTR static uint64_t evt_us_time_sync_timeout = 0;
RTC_DATA_ATTR static uint64_t evt_us_light_off = 0;
RTC_DATA_ATTR static uint64_t evt_us_disp_redraw = 0;

/* Display */
RTC_DATA_ATTR static int last_min = INT_MIN;
RTC_DATA_ATTR static int last_day = INT_MIN;
RTC_DATA_ATTR static uint64_t last_us_time_sync = 0;
RTC_DATA_ATTR static wl_status_t last_wifi_status;
RTC_DATA_ATTR static sntp_sync_status_t last_ntp_status;

void network_connect() {
  delay(10);
  WiFi.enableSTA(true);
  WiFi.mode(WIFI_STA);
  delay(10);
  WiFi.begin(CFG_WIFI_NAME, CFG_WIFI_PASS);
  delay(10);
  configTzTime(CFG_POSIX_TZ, CFG_NTP_SERVER);
  delay(10);
  return;
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

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);
  pinMode(NEOPIXEL_POWER, OUTPUT);
  rtc_gpio_pullup_en(GPIO_NUM_15); // Button A
  rtc_gpio_pullup_en(GPIO_NUM_11); // Button D
  flags = 0;

  //wakeup_reason = esp_sleep_get_wakeup_cause();

  //switch (wakeup_reason) { // Button press
  //  case ESP_SLEEP_WAKEUP_EXT0:
  //  case ESP_SLEEP_WAKEUP_EXT1: 
  //    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
  //    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
  //    esp_sleep_enable_timer_wakeup(DEBOUNCE_US); // Debounce
  //    // ePaper display does not update properly after deep sleep
  //    //esp_deep_sleep_start();
  //    esp_light_sleep_start();
  //    break;
  //}

  //if (wakeup_reason==ESP_SLEEP_WAKEUP_UNDEFINED) { // Startup
    // Configure
    display_init(true);
    if (!fatcfg_init()) {
      display_update_error("Failed to access FAT filesystem.", "", false);
      while (1) { delay(100); } // Wait forever
    }
    cfg_init(); // Create config files

    // 1. Check for Button Press
    btn = btn_read();
    if (btn) {
      // Configure
      fatcfg_msc_init();
      display_update_all("Configuration mode...", "", false);
      while (1) { // 2. Wait for button release
        btn = btn_read();
        if (btn == 0) {
          delay(DEBOUNCE_US/1000);
          btn = btn_read();
          if (btn == 0) break;
        }
        delay(DEBOUNCE_US/1000);
      }
      while (1) { // 3. Wait for Button Press
        btn = btn_read();
        if (btn != 0) {
          delay(DEBOUNCE_US/1000);
          btn = btn_read();
          if (btn != 0) break;
        }
        delay(DEBOUNCE_US/1000);
      }
    }

    cfg_init(); // Read config files
    // Sync Time over WiFi
    evt_us_time_sync_timeout = 0 + CFG_WIFI_TIMEOUT_US;
    network_connect();
    flags &= ~(FLAG_ERR_WIFI_CONN | FLAG_ERR_TIME_SYNC);
    flags |= FLAG_SYNCING;
  //}
  //else { // Deep-Sleep
  //  display_init(false);
  //  setenv("TZ", CFG_POSIX_TZ, 1);
  //  tzset(); // save the TZ variable
  //}

#ifdef CLKDBG
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting WiFi clock.");
#else
  Serial.end();
#endif
}

void loop() {
  // Buttons
  uint8_t btn;
  uint8_t btn_db;
  // Epoch Time
  struct timeval tv_now;
  uint64_t epoch_us;
  // Display Time
  struct tm now;
  char str_date[32]; // Adds some extra room for WiFi error messages.  Longest date string: "Wednesday, Sep 20, 2019" = 23 chars + \0
  char str_time[6]; // Longest time "12:00" = 5 chars + \0
  int curr_min;
  int curr_day;
  bool pm;
  sntp_sync_status_t ntp_status;

  gettimeofday(&tv_now, NULL);
  epoch_us = (uint64_t)tv_now.tv_sec*1000000UL + (uint64_t)tv_now.tv_usec;

  /* Event End */

  if (epoch_us > evt_us_light_off) {
    if (flags & (FLAG_LIGHT1|FLAG_LIGHT2)) {
#ifdef CLKDBG
      Serial.print(epoch_us);
      Serial.println(" evt end: light ");
#endif
      pixels.clear();
      digitalWrite(NEOPIXEL_POWER, HIGH); // off
      flags &= ~(FLAG_LIGHT1|FLAG_LIGHT2);
    }
  }
  if (epoch_us > evt_us_disp_redraw) {
    if (flags & FLAG_CLEAR_DISP) {
#ifdef CLKDBG
      Serial.print(epoch_us);
      Serial.println(" evt end: clear ");
      flags &= ~(FLAG_CLEAR_DISP);
#endif
    }
  }
  ntp_status = sntp_get_sync_status();
  if (ntp_status == SNTP_SYNC_STATUS_COMPLETED || 
      (flags & FLAG_SYNCING && epoch_us > evt_us_time_sync_timeout)) {
#ifdef CLKDBG
    Serial.print(epoch_us);
    Serial.println(" evt end: timesync ");
#endif
    last_ntp_status = ntp_status;
    last_wifi_status = WiFi.status();
    last_us_time_sync = (epoch_us + WIFI_POWEROFF_US); // +3s to allow WiFi to disconnect, before syncing again
    if (ntp_status != SNTP_SYNC_STATUS_COMPLETED || epoch_us < YEAR_2000_US) {
      flags |= FLAG_ERR_TIME_SYNC;
      if (last_wifi_status != WL_CONNECTED) { 
        flags |= FLAG_ERR_WIFI_CONN;
      }
    }
    flags &= ~(FLAG_SYNCING);
    WiFi.disconnect(true);
  }

  /* Event Start */

  btn = btn_read();
  if (btn) { // Debounce
    delay(DEBOUNCE_US/1000);
    btn_db = btn_read();
    btn = (btn==btn_db) ? btn : 0;
  }

  if (btn == 0x8) { // Turn on lights
    if (!(flags & FLAG_LIGHT1)) {
#ifdef CLKDBG
      Serial.print(epoch_us);
      Serial.println(" evt start: light1 ");
#endif
      evt_us_light_off = epoch_us + CFG_LIGHT1_TIMEOUT_US;
      digitalWrite(NEOPIXEL_POWER, LOW); // on
      pixels.begin();
      pixels.setBrightness(CFG_LIGHT1_BRIGHTNESS);
      pixels.fill(CFG_LIGHT1_COLOR);
      pixels.show();
      flags &= ~(FLAG_LIGHT1|FLAG_LIGHT2);
      flags |= FLAG_LIGHT1;
    }
  }
  if (btn == 0x1) { // Turn on lights
    if (!(flags & FLAG_LIGHT2)) {
#ifdef CLKDBG
      Serial.print(epoch_us);
      Serial.println(" evt start: light2 ");
#endif
      evt_us_light_off = epoch_us + CFG_LIGHT2_TIMEOUT_US;
      digitalWrite(NEOPIXEL_POWER, LOW); // on
      pixels.begin();
      pixels.setBrightness(CFG_LIGHT2_BRIGHTNESS);
      pixels.fill(CFG_LIGHT2_COLOR);
      pixels.show();
      flags &= ~(FLAG_LIGHT1|FLAG_LIGHT2);
      flags |= FLAG_LIGHT2;
    }
  }
  if (btn == 0xC) { // Clear screen for 1 minute (
    if (!(flags & FLAG_CLEAR_DISP)) {
#ifdef CLKDBG
      Serial.print(epoch_us);
      Serial.println(" evt start: clear ");
#endif
      evt_us_disp_redraw = epoch_us + 60*1000000UL;
      display_clear();
      last_min = INT_MIN; // Force full update
      flags |= FLAG_CLEAR_DISP;
    }
  }
  if (btn == 0x3 || epoch_us > (last_us_time_sync+RESYNC_US)) { // Sync time
    if (!(flags & FLAG_SYNCING)) {
#ifdef CLKDBG
      Serial.print(epoch_us);
      Serial.println(" evt start: timesync ");
#endif
      evt_us_time_sync_timeout = epoch_us + CFG_WIFI_TIMEOUT_US;
      network_connect();
      flags &= ~(FLAG_ERR_WIFI_CONN | FLAG_ERR_TIME_SYNC);
      flags |= FLAG_SYNCING;
    }
  }

  /* Update Display */

  if (!(flags & FLAG_CLEAR_DISP)) {
    getLocalTime(&now, 0);
    pm = false;

    // Date
    if (flags & FLAG_SYNCING) { curr_day = -1;
      strcpy(str_date, "Syncing time...");
    }
    else if (flags & FLAG_ERR_WIFI_CONN) { curr_day = -2;
      strcpy(str_date, "No WiFi connection!");
      switch (last_wifi_status) {
        case WL_NO_SHIELD:
          strcpy(str_date, "WiFi Missing Shield");
          break;
        case WL_IDLE_STATUS:
          strcpy(str_date, "WiFi Idle");
          break;
        case WL_NO_SSID_AVAIL:
          strcpy(str_date, "WiFi Network Unavailable");
          break;
        case WL_SCAN_COMPLETED:
          strcpy(str_date, "WiFi Scan Completed");
          break;
        case WL_CONNECTED:
          strcpy(str_date, "WiFi Connected");
          break;
        case WL_CONNECT_FAILED:
          strcpy(str_date, "WiFi Connection Failed");
          break;
        case WL_CONNECTION_LOST:
          strcpy(str_date, "WiFi Connection Lost");
          break;
        case WL_DISCONNECTED:
          strcpy(str_date, "WiFi Disconnected");
          break;
        default:
          strcpy(str_date, "WiFi Unknown Error");
          break;
      }
    }
    else if (flags & FLAG_ERR_TIME_SYNC) { curr_day = -3;
      switch (last_ntp_status) {
        case SNTP_SYNC_STATUS_COMPLETED:
          strcpy(str_date, "Time Sync Completed?");
        case SNTP_SYNC_STATUS_IN_PROGRESS:
          strcpy(str_date, "Time Sync In Smooth Mode");
        case SNTP_SYNC_STATUS_RESET:
          strcpy(str_date, "Time Sync In Reset");
        default:
          strcpy(str_date, "Unable to sync time!");
      }
    }
    else { curr_day = now.tm_yday;
      strftime(str_date, sizeof(str_date), "%A, %b %e %G", &now);
    }
    // Time
    if (epoch_us < YEAR_2000_US) { curr_min = -1;
      strcpy(str_time, "");
    }
    else if (CFG_24_HOUR) { curr_min = now.tm_min;
      strftime(str_time, sizeof(str_time), "%H:%M", &now);
    }
    else { curr_min = now.tm_min;
      strftime(str_time, sizeof(str_time), "%l:%M", &now);
      if (now.tm_hour >= 12) pm = true;
    }

    if (curr_day != last_day || (curr_min != last_min && curr_min % 5 == 0)) {
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
      last_day = curr_day;
      last_min = curr_min;
    }
  }

  gettimeofday(&tv_now, NULL);
  epoch_us = (uint64_t)tv_now.tv_sec*1000000UL + (uint64_t)tv_now.tv_usec;

  /* Sleep */

  uint64_t evt_next = epoch_us + (60*1000000UL) - (epoch_us % (60*1000000UL)); // next display update
  if (evt_us_light_off         > epoch_us && evt_next > evt_us_light_off) {
    evt_next = evt_us_light_off;
  }
  if (evt_us_time_sync_timeout > epoch_us && evt_next > evt_us_time_sync_timeout) {
    evt_next = evt_us_time_sync_timeout;
  }
  uint64_t sleep_us = evt_next - epoch_us;

  if (flags & FLAG_SYNCING) {
    delay(10);
  }
  else { // Sleep
#ifdef CLKDBG
    Serial.flush();
#endif
    digitalWrite(SPEAKER_SHUTDOWN, LOW); // off
    //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // needed to keep button pullups enabled, NEOPIXEL_POWER HIGH in deep sleep
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0); // Wake on button A press
    esp_sleep_enable_ext1_wakeup(1<<11, ESP_EXT1_WAKEUP_ALL_LOW); // Wake on button D press
    esp_sleep_enable_timer_wakeup(sleep_us); // Enter sleep until next event
    if (epoch_us < evt_us_light_off) {
      //esp_light_sleep_start();
    }
    else {
      // ePaper display does not update properly after deep sleep
      //esp_deep_sleep_start();
      esp_light_sleep_start();
    }
  }
}

