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
- synchronize in the background from WWVB when sync is enabled
- allow user settings through a small button-driven UI
- store RTC time in UTC and apply timezone/DST only for display

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
- `DISPLAY_SET_UTC_TIME`
- `DISPLAY_SET_TIMEZONE`
- `DISPLAY_SET_DST`
- `DISPLAY_SET_SYNC`

### SettingItem

`SettingItem` identifies the active field or setting being edited.

- `SETTING_UTC_HOUR`
- `SETTING_UTC_MINUTE`
- `SETTING_UTC_SECOND`
- `SETTING_TIMEZONE`
- `SETTING_DST`
- `SETTING_SYNC_ENABLE`

### State relationship

- the clock can be syncing while showing `DISPLAY_CLOCK` or `DISPLAY_STATUS`
- the settings UI uses one UTC time renderer with an active-field selector for hour, minute, or second
- background sync does not block the active display
- no new sync starts while in settings
- an already-active sync is allowed to finish while in settings

## Display renderer design

`display.cpp` uses explicit view renderers:

- `draw_clock_view()`
- `draw_status_view()`
- `draw_utc_time_view()`
- `draw_timezone_view()`
- `draw_toggle_view()`

`display_handler()` dispatches to the appropriate renderer based on `DisplayState`.

The settings UI is intentionally compact to conserve flash and avoid helper text. The current settings screens show only:

- `HH:MM:SS UTC`
- `UTC±offset`
- `DST ON/OFF`
- `SYNC ON/OFF`

Only the active field blinks.

## Button model

### Outside settings

- `MENU` short toggles between `DISPLAY_CLOCK` and `DISPLAY_STATUS`
- `MENU` long enters settings at `SETTING_UTC_HOUR`
- `SYNC` short starts WWVB sync only when sync is enabled and the receiver is idle
- `UP` and `DOWN` are inert

### In settings

#### UTC time group

- `UP/DOWN` wrap-adjust the active UTC field
- `MENU` short advances `HH -> MM -> SS -> HH`
- `MENU` long commits the pending UTC time only when pending `SYNC` is `OFF`, then advances to timezone
- while editing UTC time, the displayed values are detached from the live RTC

#### Timezone

- `UP/DOWN` wrap-adjust timezone from `-12` to `+14`
- `MENU` short commits timezone and advances to DST
- `MENU` long has no special meaning

#### DST

- `UP/DOWN` toggle pending DST
- `MENU` short commits DST and advances to SYNC
- `MENU` long has no special meaning

#### SYNC

- `UP/DOWN` toggle pending sync enable
- `MENU` short commits SYNC and exits settings to the normal clock view
- `MENU` long has no special meaning

## Sync and manual time interaction

- RTC time is stored in UTC
- manual UTC setting is committed only if pending `SYNC` is `OFF`
- if pending `SYNC` is `ON`, long-pressing `MENU` during UTC editing skips the RTC write and still advances to timezone
- if a WWVB sync completes while settings are active, the RTC is updated, but the displayed pending edit values remain unchanged

## Timing model

### Main loop

Foreground code is responsible for:

- servicing WWVB background sync
- consuming button events
- updating the display
- refreshing the settings screen at the blink interval even when RTC time is not changing

### ISRs

- external interrupt from ES100 for WWVB timing
- Timer2 compare interrupt at 1 kHz for button debounce

Design rule: keep ISRs short and deterministic. No display, Serial, or I2C work in ISRs.

## ES100 design

The ES100 driver owns its own receive state machine and should remain conservative because startup and IRQ sequencing are timing-sensitive.

The ES100 public API is intentionally minimal:

- `es100_init()`
- `es100_start_receive()`
- `es100_service()`

Low-level register inspection helpers stay file-local inside `es100.cpp`. The driver also applies IRQ-latency correction internally before returning a successful UTC time to the application.

## Future patch guidance

When future architectural patches are made, update:

- `README.md`
- `TEST_PLAN.md`
- `DESIGN.md`
