#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <Arduino.h>

// Default test/development pinout. Adjust to match the actual hardware.
// Buttons are expected to be wired as INPUT_PULLUP: pressed = LOW.
constexpr uint8_t MENU_BTN_PIN = A0;
constexpr uint8_t UP_BTN_PIN   = A1;
constexpr uint8_t DOWN_BTN_PIN = A2;
constexpr uint8_t SYNC_BTN_PIN = A3;

constexpr uint16_t DEBOUNCE_MS_THRESHOLD = 12;   // ~12 ms stable state required
constexpr uint16_t LONGPRESS_MS_THRESHOLD = 700; // 700 ms long press

enum ButtonId : uint8_t {
  BUTTON_MENU = 0,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_SYNC,
  BUTTON_COUNT
};

enum ButtonEvent : uint8_t {
  BUTTON_EVENT_NONE    = 0,
  BUTTON_EVENT_SHORT   = 1 << 0,
  BUTTON_EVENT_LONG    = 1 << 1,
  BUTTON_EVENT_RELEASE = 1 << 2
};

void debounce_init(void);

// Event accessors. These are safe to call from foreground code.
uint8_t debounce_take_events(ButtonId id);
bool debounce_is_down(ButtonId id);
uint16_t debounce_hold_ms(ButtonId id);

#endif  // DEBOUNCE_H
