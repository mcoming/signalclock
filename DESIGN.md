# DESIGN.md

## Purpose

This document describes the current architecture and intended design of SignalClock so a software engineer can get oriented quickly.

## High-level design

SignalClock is an AVR-based clock with:

- Arduino Pro Mini (8 MHz)
- ES100 WWVB receiver
- DS3231 RTC
- SSD1306 OLED display
- deterministic button sampling via Timer2 ISR

The clock is intended to:

- show RTC time immediately at boot
- synchronize in the background from WWVB
- allow user settings through a small button-driven UI

## Modules

| File | Responsibility |
|---|---|
| `signalclock.ino` | minimal Arduino entry wrapper (`setup()` / `loop()`) |
| `signalclock_app.cpp/.h` | top-level control flow, states, button dispatch, sync orchestration |
| `display.cpp/.h` | rendering of display views |
| `es100.cpp/.h` | WWVB receiver control and state machine |
| `debounce.cpp/.h` | deterministic button sampling and event generation |

## Hardware architecture

```text
FT232 -> Pro Mini -> I2C bus -> DS3231
                      |        -> ES100
                      |        -> SSD1306 OLED
                      +-> Buttons
```

## Pin mapping

| Function | Pin |
|---|---:|
| ES100 IRQ | D3 |
| ES100 enable | D4 |
| MENU | A0 |
| UP | A1 |
| DOWN | A2 |
| SYNC | A3 |
| I2C SDA | A4 |
| I2C SCL | A5 |

Buttons use `INPUT_PULLUP`; pressed = LOW.

## Runtime model

### OperatingState

`OperatingState` describes what the clock is doing.

- `OPERATING_RUNNING`
- `OPERATING_SYNCING`
- `OPERATING_SETTING`

### DisplayState

`DisplayState` describes which view is being rendered.

- `DISPLAY_CLOCK`
- `DISPLAY_STATUS`
- `DISPLAY_SET_TIMEZONE`
- `DISPLAY_SET_DST`
- `DISPLAY_SET_EXIT`

### State relationship

- the clock can be syncing while showing `DISPLAY_CLOCK` or `DISPLAY_STATUS`
- settings views are rendered explicitly by `DisplayState`
- background sync should not block the active display

## Display renderer design

`display.cpp` uses explicit view renderers:

- `draw_clock_view()`
- `draw_status_view()`
- `draw_timezone_view()`
- `draw_dst_view()`
- `draw_exit_view()`

`display_handler()` dispatches to the appropriate renderer based on `DisplayState`.

## Button model

### Outside settings

- `MENU` short toggles between `DISPLAY_CLOCK` and `DISPLAY_STATUS`
- `MENU` long enters settings
- `SYNC` short starts WWVB sync if idle, otherwise reports busy
- `UP` and `DOWN` are inert

### In settings

#### `DISPLAY_SET_TIMEZONE`
- `UP/DOWN` adjust pending timezone
- `MENU` short commits timezone and advances to DST
- `MENU` long cancels timezone edits

#### `DISPLAY_SET_DST`
- `UP/DOWN` toggle pending DST
- `MENU` short commits DST and advances to Exit
- `MENU` long cancels DST edits

#### `DISPLAY_SET_EXIT`
- `MENU` short exits settings
- `UP/DOWN` returns to DST

## Timing model

### Main loop

Foreground code is responsible for:

- servicing WWVB background sync
- consuming button events
- updating the display

### ISRs

- external interrupt from ES100 for WWVB timing
- Timer2 compare interrupt at 1 kHz for button debounce

Design rule: keep ISRs short and deterministic. No display, Serial, or I2C work in ISRs.

## ES100 design

The ES100 driver owns its own receive state machine and should remain conservative because startup and IRQ sequencing are timing-sensitive.

The ES100 public API should remain minimal. Low-level register inspection helpers that are only useful for debugging should stay file-local inside `es100.cpp` unless the application truly needs them.

## Future patch guidance

When future architectural patches are made, update:

- `README.md`
- `TEST_PLAN.md`
- `DESIGN.md`

## Sketch refactor note

The project now keeps `signalclock.ino` intentionally tiny. The application logic lives in `signalclock_app.cpp/.h`, which improves static-analysis behavior and keeps the Arduino-specific entry file minimal.
