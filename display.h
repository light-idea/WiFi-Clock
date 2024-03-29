#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#define DISPLAY_BAUD 115200

#define DISPLAY_W 296
#define DISPLAY_H 128
#define DISPLAY_R 3

#define PADDING_TOP 1
#define PADDING_BOT 11
#define TIME_OFFSET_X -6
#define DATE_OFFSET_X -5

#define PM_WIDTH 12
#define PM_HEIGHT 12

#define DATE_HEIGHT 32
#define TIME_HEIGHT (DISPLAY_H-DATE_HEIGHT)

void display_init(bool initial=true);
void display_clear();
void display_update_error(const char* error, const char* time, bool pm);
void display_update_all(const char* date, const char* time, bool pm);
void display_update_time(const char* time, bool pm);

#endif
