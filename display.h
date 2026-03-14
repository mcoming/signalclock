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
// Shared UI constants
// -----------------------------------------------------------------------------
enum DisplayView : uint8_t {
  DISPLAY_CLOCK = 0,
  DISPLAY_STATUS,
  DISPLAY_SET_UTC_TIME,
  DISPLAY_SET_TIMEZONE,
  DISPLAY_SET_DST,
  DISPLAY_SET_SYNC
};

enum SettingItemDisplay : uint8_t {
  SETTING_UTC_HOUR = 0,
  SETTING_UTC_MINUTE,
  SETTING_UTC_SECOND,
  SETTING_TIMEZONE,
  SETTING_DST,
  SETTING_SYNC_ENABLE
};

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
                     uint16_t wwvb_elapsed_sec, uint8_t operating_state,
                     uint8_t display_state, uint8_t current_setting_item,
                     int8_t timezone_hours, bool dst_enabled,
                     uint8_t pending_utc_hour, uint8_t pending_utc_minute,
                     uint8_t pending_utc_second, int8_t pending_timezone_hours,
                     bool pending_dst_enabled, bool sync_enabled,
                     bool pending_sync_enabled, bool blink_visible);

#endif // DISPLAY_H
