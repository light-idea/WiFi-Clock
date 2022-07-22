#include <GxEPD2_BW.h>

#include "fonts/NotoSerif_Regular10pt7b.h"
#include "fonts/NotoSerif_Regular56pt7b.h"

#include "display.h"
#include "config.h"

// Peripherals
GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(8, 7, 6, 5)); // GDEW029T5
GFXcanvas16 canvas(DISPLAY_W, DISPLAY_H);

void display_init() {
  display.init(115200);
  display.setRotation(1);
}

void display_clear() {
  display.clearScreen();
  display.hibernate();
}

void display_update_all(char* date, char* time) {
  int16_t x, y;
  uint16_t w, h;
  int16_t date_x, date_y, time_x, time_y;
  canvas.setRotation(0); // Reset Rotation
  canvas.setFont(&NotoSerif_Regular10pt7b);
  canvas.getTextBounds(date, 0, 0, &x, &y, &w, &h); //calc width of new string
  date_x = (DISPLAY_W/2 - w/2) + DATE_OFFSET_X;
  date_y = h + PADDING_TOP;
  canvas.setFont(&NotoSerif_Regular56pt7b);
  canvas.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    // Date
    display.setFont(&NotoSerif_Regular10pt7b);
    display.setCursor(date_x, date_y);
    display.print(date);
    // Time
    display.setFont(&NotoSerif_Regular56pt7b);
    display.setCursor(time_x, time_y);
    display.print(time);
    if (HOUR_12_24_N) {
      display.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                            PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display.nextPage());
}

void display_update_time(char* time) {
  int16_t x, y;
  uint16_t w, h;
  int16_t time_x, time_y;
  canvas.setRotation(0); // Reset Rotation
  canvas.setFont(&NotoSerif_Regular56pt7b);
  canvas.getTextBounds(time, 0, 0, &x, &y, &w, &h);
  time_x = (DISPLAY_W/2 - w/2) + TIME_OFFSET_X;
  time_y = DISPLAY_H - PADDING_BOT;

  display.setPartialWindow(0, 32, DISPLAY_W, DISPLAY_H-32);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&NotoSerif_Regular56pt7b);
    display.setCursor(time_x, time_y);
    display.print(time);
    if (HOUR_12_24_N) {
      display.fillRect(DISPLAY_W-PM_WIDTH, DISPLAY_H-PM_HEIGHT, 
                      PM_WIDTH, PM_HEIGHT, GxEPD_BLACK);
    }
  } while (display.nextPage());
}
