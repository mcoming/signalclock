# SignalClock

SignalClock is an AVR-based radio-controlled clock built around an Arduino Pro Mini, an ES100 WWVB receiver, a DS3231 RTC, and an SSD1306 OLED display.

The clock is designed to show time immediately from the RTC at boot, then synchronize in the background from WWVB. The firmware includes a hierarchical operating/display state model and explicit display renderers for the normal clock view, a status view, and settings views.

## Features

- Arduino Pro Mini firmware target
- DS3231 RTC for local timekeeping
- ES100 WWVB receiver for radio synchronization
- SSD1306 OLED display
- Immediate clock display at startup
- Background WWVB synchronization
- Deterministic 1 kHz Timer2-based button sampling and debounce
- Explicit display renderers for clock, status, timezone, DST, and exit views
- Modular source layout for display, ES100, and button handling

## Hardware

- **Microcontroller:** Arduino Pro Mini
- **Clock speed:** 8 MHz
- **RTC:** DS3231
- **WWVB receiver:** ES100
- **Display:** SSD1306 OLED
- **Programming adapter:** FT232 USB-to-serial

## Buttons

Current button mapping assumes `INPUT_PULLUP` wiring:

- `MENU` -> `A0`
- `UP` -> `A1`
- `DOWN` -> `A2`
- `SYNC` -> `A3`

One side of each switch goes to the MCU pin and the other side goes to GND. Pressed = LOW.

## Current behavior

### Operating states

- `OPERATING_RUNNING`
- `OPERATING_SYNCING`
- `OPERATING_SETTING`

### Display states

- `DISPLAY_CLOCK`
- `DISPLAY_STATUS`
- `DISPLAY_SET_TIMEZONE`
- `DISPLAY_SET_DST`
- `DISPLAY_SET_EXIT`

### Normal operation

- RTC time is displayed immediately at boot.
- WWVB synchronization starts in the background.
- `MENU` short toggles between `DISPLAY_CLOCK` and `DISPLAY_STATUS`.
- `MENU` long enters settings.
- `SYNC` short starts sync if idle, otherwise reports busy.
- `UP` and `DOWN` are intentionally inert outside settings.

### Settings behavior

- `DISPLAY_SET_TIMEZONE`: `UP/DOWN` adjust timezone, `MENU` short commits and advances.
- `DISPLAY_SET_DST`: `UP/DOWN` toggle DST, `MENU` short commits and advances.
- `DISPLAY_SET_EXIT`: `MENU` short exits settings, `UP/DN` returns to DST.
- `MENU` long cancels the current setting item.

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
  TEST_PLAN.md
  DESIGN.md
  .gitignore
```

## Build environment

Recommended Arduino IDE settings:

- **Board:** Arduino Pro or Pro Mini
- **Processor:** ATmega328P (3.3V, 8 MHz)
- **Serial Monitor baud:** 9600

## Development notes

- Keep ISRs short and deterministic.
- Avoid `String` on AVR.
- Prefer non-blocking foreground logic.
- Update `README.md`, `TEST_PLAN.md`, and `DESIGN.md` with each architectural patch.
