#include "display.h"

#include <Wire.h>
#include <math.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// OLED object
// -----------------------------------------------------------------------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -----------------------------------------------------------------------------
// File-local constants
// -----------------------------------------------------------------------------
namespace {

const char PRODUCT[] PROGMEM = "oled_clock_es100.ino";
const char VERSION[] PROGMEM = "040525";

const char SUN_STR[] PROGMEM = "Sun";
const char MON_STR[] PROGMEM = "Mon";
const char TUE_STR[] PROGMEM = "Tue";
const char WED_STR[] PROGMEM = "Wed";
const char THU_STR[] PROGMEM = "Thu";
const char FRI_STR[] PROGMEM = "Fri";
const char SAT_STR[] PROGMEM = "Sat";

const char JAN_STR[] PROGMEM = "Jan";
const char FEB_STR[] PROGMEM = "Feb";
const char MAR_STR[] PROGMEM = "Mar";
const char APR_STR[] PROGMEM = "Apr";
const char MAY_STR[] PROGMEM = "May";
const char JUN_STR[] PROGMEM = "Jun";
const char JUL_STR[] PROGMEM = "Jul";
const char AUG_STR[] PROGMEM = "Aug";
const char SEP_STR[] PROGMEM = "Sep";
const char OCT_STR[] PROGMEM = "Oct";
const char NOV_STR[] PROGMEM = "Nov";
const char DEC_STR[] PROGMEM = "Dec";

PGM_P const DAYS_OF_WEEK[] PROGMEM = {SUN_STR, MON_STR, TUE_STR, WED_STR,
                                      THU_STR, FRI_STR, SAT_STR};

PGM_P const MONTHS_OF_YEAR[] PROGMEM = {JAN_STR, FEB_STR, MAR_STR, APR_STR,
                                        MAY_STR, JUN_STR, JUL_STR, AUG_STR,
                                        SEP_STR, OCT_STR, NOV_STR, DEC_STR};

constexpr int16_t CLOCK_CENTER_X = 32;
constexpr int16_t CLOCK_CENTER_Y = 32;
constexpr int16_t CLOCK_RADIUS = 30;

void copy_progmem_string(PGM_P src, char *dst, size_t dst_size) {
  if (dst_size == 0) {
    return;
  }

  strncpy_P(dst, src, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

void draw_hand(uint8_t value, uint8_t max_value, int16_t length) {
  const float angle = (2.0f * PI * value / max_value) - (PI / 2.0f);
  const int16_t x2 = CLOCK_CENTER_X + (int16_t)(cos(angle) * length);
  const int16_t y2 = CLOCK_CENTER_Y + (int16_t)(sin(angle) * length);

  display.drawLine(CLOCK_CENTER_X, CLOCK_CENTER_Y, x2, y2, SSD1306_WHITE);
}

void draw_graduations() {
  for (uint8_t i = 0; i < 60; i++) {
    const float angle = (2.0f * PI * i / 60.0f) - (PI / 2.0f);

    const int16_t outer_x =
        CLOCK_CENTER_X + (int16_t)(cos(angle) * CLOCK_RADIUS);
    const int16_t outer_y =
        CLOCK_CENTER_Y + (int16_t)(sin(angle) * CLOCK_RADIUS);

    const int16_t inner_len =
        (i % 5 == 0) ? (CLOCK_RADIUS - 4) : (CLOCK_RADIUS - 2);
    const int16_t inner_x = CLOCK_CENTER_X + (int16_t)(cos(angle) * inner_len);
    const int16_t inner_y = CLOCK_CENTER_Y + (int16_t)(sin(angle) * inner_len);

    display.drawLine(inner_x, inner_y, outer_x, outer_y, SSD1306_WHITE);
  }
}

void draw_status_line(bool rtc_set, bool wwvb_active, bool wwvb_done,
                      bool wwvb_failed, bool wwvb_timed_out,
                      uint16_t wwvb_elapsed_sec) {
  char line[17];

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(64, 44);

  if (wwvb_active) {
    // Keep it compact so it resembles the original simple status line.
    snprintf(line, sizeof(line), "WWVB %3us", wwvb_elapsed_sec);
    display.print(line);
  } else if (wwvb_failed) {
    display.print(F("WWVB failed"));
  } else if (wwvb_timed_out) {
    display.print(F("WWVB timeout"));
  } else if (rtc_set) {
    display.print(F("RTC synced"));
  } else if (wwvb_done) {
    display.print(F("RTC running"));
  } else {
    display.print(F("WWVB wait"));
  }
}

} // namespace

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup_display(void) {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);
    }
  }

  display.clearDisplay();
  display.setTextWrap(false);
  display.display();

  display_msg(F("OLED found."), 0, 0, SSD1306_WHITE, SSD1306_BLACK, 1);
  delay(500);
}

// -----------------------------------------------------------------------------
// Text helpers
// -----------------------------------------------------------------------------
void draw_msg(const char *str, uint8_t x, uint8_t y, uint16_t fg, uint16_t bg,
              uint8_t size) {
  display.setTextSize(size);
  display.setTextColor(fg, bg);
  display.setCursor(x, y);
  display.print(str);
}

void draw_msg(const __FlashStringHelper *str, uint8_t x, uint8_t y, uint16_t fg,
              uint16_t bg, uint8_t size) {
  display.setTextSize(size);
  display.setTextColor(fg, bg);
  display.setCursor(x, y);
  display.print(str);
}

void display_msg(const char *str, uint8_t x, uint8_t y, uint16_t fg,
                 uint16_t bg, uint8_t size) {
  display.clearDisplay();
  draw_msg(str, x, y, fg, bg, size);
  display.display();
}

void display_msg(const __FlashStringHelper *str, uint8_t x, uint8_t y,
                 uint16_t fg, uint16_t bg, uint8_t size) {
  display.clearDisplay();
  draw_msg(str, x, y, fg, bg, size);
  display.display();
}

// -----------------------------------------------------------------------------
// Time/date drawing
// -----------------------------------------------------------------------------
void draw_time(const DateTime &now) {
  char buf[9];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", now.hour(), now.minute(),
           now.second());

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(64, 8);
  display.print(buf);
}

void draw_date(const DateTime &now) {
  char day_buf[4];
  char month_buf[4];
  char line[16];

  uint8_t dow = now.dayOfTheWeek();
  if (dow > 6) {
    dow = 0;
  }

  uint8_t month = now.month();
  if (month < 1 || month > 12) {
    month = 1;
  }

  copy_progmem_string((PGM_P)pgm_read_ptr(&DAYS_OF_WEEK[dow]), day_buf,
                      sizeof(day_buf));
  copy_progmem_string((PGM_P)pgm_read_ptr(&MONTHS_OF_YEAR[month - 1]),
                      month_buf, sizeof(month_buf));

  snprintf(line, sizeof(line), "%s %s %2u", day_buf, month_buf, now.day());

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(64, 30);
  display.print(line);
}

// -----------------------------------------------------------------------------
// Main display handler
// -----------------------------------------------------------------------------
void display_handler(const DateTime &now, bool rtc_set, bool wwvb_active,
                     bool wwvb_done, bool wwvb_failed, bool wwvb_timed_out,
                     uint16_t wwvb_elapsed_sec) {
  display.clearDisplay();

  // Left side analog clock, matching the original feel.
  display.drawCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS,
                     SSD1306_WHITE);
  draw_graduations();

  draw_hand(now.hour() % 12, 12, 16);
  draw_hand(now.minute(), 60, 22);
  draw_hand(now.second(), 60, 26);

  display.fillCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, 2, SSD1306_WHITE);

  // Right side digital time/date, similar to the original layout.
  draw_time(now);
  draw_date(now);

  // Small status line near the bottom, original-style.
  draw_status_line(rtc_set, wwvb_active, wwvb_done, wwvb_failed, wwvb_timed_out,
                   wwvb_elapsed_sec);

  // Footer.
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setCursor(64, 54);
  display.print((const __FlashStringHelper *)PRODUCT);
  display.print(F(" "));
  display.print((const __FlashStringHelper *)VERSION);

  display.display();
}
