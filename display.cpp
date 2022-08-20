#include <GxEPD2_BW.h>

//#include "fonts/NotoSerif_Regular10pt7bGrey.h"
//#include "fonts/NotoSerif_Regular56pt7bGrey.h"
#include "fonts/NotoSerif_Regular10pt7b.h"
#include "fonts/NotoSerif_Regular56pt7b.h"

#include "display.h"
#include "cfg.h"

// Peripherals
GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5

void display_init() {
  display.init(DISPLAY_BAUD);
  display.setRotation(DISPLAY_R);
}

void display_clear() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
}

void display_update_all(const char* date, const char* time, bool pm) {
  int16_t x, y;
  uint16_t w, h;
  int16_t date_x, date_y, time_x, time_y;
  display.setFont(&NotoSerif_Regular10pt7b);
  display.getTextBounds(date, 0, 0, &x, &y, &w, &h); //calc width of new string
  date_x = (DISPLAY_W/2 - w/2) + DATE_OFFSET_X;
  date_y = h + PADDING_TOP;
  display.setFont(&NotoSerif_Regular56pt7b);
  display.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // For Grayscale
    //for (int i = 2; i >= 0; --i) { // Date
    //  display.setTextColor(
    //      (i==2) ? GxEPD_LIGHTGREY : 
    //      (i==1) ? GxEPD_DARKGREY : 
    //      GxEPD_BLACK
    //  );
    //  display.setFont(&NotoSerif_Regular10pt7bGrey[i]);
    //  display.setCursor(date_x, date_y);
    //  display.print(date);
    //}
    //for (int i = 2; i >= 0; --i) { // Time
    //  display.setTextColor(
    //      (i==2) ? GxEPD_LIGHTGREY : 
    //      (i==1) ? GxEPD_DARKGREY : 
    //      GxEPD_BLACK
    //  );
    //  display.setFont(&NotoSerif_Regular56pt7bGrey[i]);
    //  display.setCursor(time_x, time_y);
    //  display.print(time);
    //}
    display.setTextColor(GxEPD_BLACK);
    // Date
    display.setFont(&NotoSerif_Regular10pt7b);
    display.setCursor(date_x, date_y);
    display.print(date);
    // Time
    display.setFont(&NotoSerif_Regular56pt7b);
    display.setCursor(time_x, time_y);
    display.print(time);
    if (pm) {
      display.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                            PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display.nextPage());
}

void display_update_error(const char* error, const char* time, bool pm) {
  int16_t x, y;
  uint16_t w, h;
  int16_t error_x, error_y, time_x, time_y;
  display.setFont(&NotoSerif_Regular10pt7b);
  display.getTextBounds(error, 0, 0, &x, &y, &w, &h); //calc width of new string
  error_x = (DISPLAY_W/2 - w/2) + DATE_OFFSET_X;
  error_y = h + PADDING_TOP;
  display.setFont(&NotoSerif_Regular56pt7b);
  display.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // Error
    display.fillRect(0, 0, DISPLAY_W, DATE_HEIGHT, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&NotoSerif_Regular10pt7b);
    display.setCursor(error_x, error_y);
    display.print(error);
    // Time
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&NotoSerif_Regular56pt7b);
    display.setCursor(time_x, time_y);
    display.print(time);
    if (pm) {
      display.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                            PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display.nextPage());
}

void display_update_time(const char* time, bool pm) {
  int16_t x, y;
  uint16_t w, h;
  int16_t time_x, time_y;
  display.setFont(&NotoSerif_Regular56pt7b);
  display.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display.setPartialWindow(0, DATE_HEIGHT, DISPLAY_W, TIME_HEIGHT);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&NotoSerif_Regular56pt7b);
    display.setCursor(time_x, time_y);
    display.print(time);
    if (pm) {
      display.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                      PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display.nextPage());
}

