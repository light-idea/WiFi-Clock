#include <GxEPD2_4G_4G.h>
#include <GxEPD2_4G_BW.h>

#include "fonts/NotoSerif_Regular10pt7bGrey.h"
#include "fonts/NotoSerif_Regular56pt7bGrey.h"
#include "fonts/NotoSerif_Regular56pt7b.h"

#include "display.h"
#include "config.h"

// Peripherals
GxEPD2_4G_4G<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display_4g(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5
GxEPD2_4G_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display_bw(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5

void display_clear() {
  display_4g.init(115200);
  display_4g.setRotation(DISPLAY_ROTATION);
  display_4g.clearScreen();
  display_4g.hibernate();
  display_4g.powerOff();
}

void display_update_all(char* date, char* time) {
  int16_t x, y;
  uint16_t w, h;
  int16_t date_x, date_y, time_x, time_y;
  display_4g.setFont(&NotoSerif_Regular10pt7bGrey[0]);
  display_4g.getTextBounds(date, 0, 0, &x, &y, &w, &h); //calc width of new string
  date_x = (DISPLAY_W/2 - w/2) + DATE_OFFSET_X;
  date_y = h + PADDING_TOP;
  display_4g.setFont(&NotoSerif_Regular56pt7bGrey[0]);
  display_4g.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display_4g.init(115200);
  display_4g.setRotation(DISPLAY_ROTATION);
  display_4g.setFullWindow();
  display_4g.firstPage();
  do {
    display_4g.fillScreen(GxEPD_WHITE);
    // Date
    for (int i = 2; i >= 0; --i) {
      display_4g.setTextColor((i==2) ? GxEPD_LIGHTGREY : (i==1) ? GxEPD_DARKGREY : GxEPD_BLACK);
      display_4g.setFont(&NotoSerif_Regular10pt7bGrey[i]);
      display_4g.setCursor(date_x, date_y);
      display_4g.print(date);
    }
    // Time
    for (int i = 2; i >= 0; --i) {
      display_4g.setTextColor((i==2) ? GxEPD_LIGHTGREY : (i==1) ? GxEPD_DARKGREY : GxEPD_BLACK);
      display_4g.setFont(&NotoSerif_Regular56pt7bGrey[i]);
      display_4g.setCursor(time_x, time_y);
      display_4g.print(time);
    }
    if (HOUR_12_24_N) {
      display_4g.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                            PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display_4g.nextPage());
  display_4g.hibernate();
  display_4g.powerOff();
}

void display_update_time(char* time) {
  int16_t x, y;
  uint16_t w, h;
  int16_t time_x, time_y;
  display_bw.setFont(&NotoSerif_Regular56pt7bGrey[0]);
  display_bw.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display_bw.init(115200);
  display_bw.setRotation(DISPLAY_ROTATION);
  display_bw.setPartialWindow(0, 30, DISPLAY_W, DISPLAY_H-30);
  display_bw.firstPage();
  do {
    display_bw.fillScreen(GxEPD_WHITE);
    display_bw.setTextColor(GxEPD_BLACK);
    display_bw.setFont(&NotoSerif_Regular56pt7b);
    display_bw.setCursor(time_x, time_y);
    display_bw.print(time);
    if (HOUR_12_24_N) {
      display_bw.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                            PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display_bw.nextPage());
  display_bw.hibernate();
  display_bw.powerOff();
}
