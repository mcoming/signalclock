#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <RTClib.h>
#include <avr/pgmspace.h>

// -----------------------------------------------------------------------------
// Display geometry
// -----------------------------------------------------------------------------
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1;

// -----------------------------------------------------------------------------
// Global display object
// -----------------------------------------------------------------------------
extern Adafruit_SSD1306 display;

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
void setup_display(void);

void draw_msg(const char *str, uint8_t x, uint8_t y, uint16_t fg, uint16_t bg,
              uint8_t size);

void draw_msg(const __FlashStringHelper *str, uint8_t x, uint8_t y, uint16_t fg,
              uint16_t bg, uint8_t size);

void display_msg(const char *str, uint8_t x, uint8_t y, uint16_t fg,
                 uint16_t bg, uint8_t size);

void display_msg(const __FlashStringHelper *str, uint8_t x, uint8_t y,
                 uint16_t fg, uint16_t bg, uint8_t size);

void draw_time(const DateTime &now);
void draw_date(const DateTime &now);

void display_handler(const DateTime &now, bool rtc_set, bool wwvb_active,
                     bool wwvb_done, bool wwvb_failed, bool wwvb_timed_out,
                     uint16_t wwvb_elapsed_sec);

#endif // DISPLAY_H
