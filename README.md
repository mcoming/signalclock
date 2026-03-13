# SignalClock

SignalClock is an AVR-based radio-controlled clock built around an Arduino Pro Mini, an ES100 WWVB receiver, a DS3231 RTC, and an SSD1306 OLED display.

The project is designed to show time immediately from the RTC at boot, then synchronize in the background from WWVB. The display combines an analog clock face with digital time/date readout, and the firmware is being extended with deterministic button handling for user settings and manual actions.

---

## Features

- Arduino Pro Mini firmware target
- DS3231 RTC for local timekeeping
- ES100 WWVB receiver for radio synchronization
- SSD1306 OLED display
- Immediate clock display at startup
- Background WWVB synchronization
- Sync status indicator on the display
- Deterministic 1 kHz Timer2-based button sampling and debounce
- Modular source layout for display, ES100, and button handling

---

## Hardware

Current project context:

- **Microcontroller:** Arduino Pro Mini
- **Clock speed:** 8 MHz
- **RTC:** DS3231
- **WWVB receiver:** ES100
- **Display:** SSD1306 OLED
- **Programming adapter:** FT232 USB-to-serial

---

## Firmware behavior

Current firmware behavior is based on the following model:

1. Initialize display and RTC
2. Show RTC time immediately
3. Start WWVB synchronization in the background
4. If WWVB sync succeeds, update the RTC
5. Continue normal time display
6. Future revisions will add user-controlled settings and scheduled re-sync behavior

This avoids long startup delays and keeps the clock usable even when radio reception is poor.

---

## Project structure

```text
signalclock/
  signalclock.ino
  display.cpp
  display.h
  es100.cpp
  es100.h
  debounce.cpp
  debounce.h
  README.md
  .gitignore
