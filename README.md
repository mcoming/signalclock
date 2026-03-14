# SignalClock

SignalClock is an AVR-based radio-controlled clock built around an Arduino Pro Mini, an ES100 WWVB receiver, a DS3231 RTC, and an SSD1306 OLED display.

The clock is designed to show time immediately from the RTC at boot, then synchronize in the background from WWVB when sync is enabled. The firmware includes a hierarchical operating/display state model and explicit display renderers for the normal clock view, a status view, and compact settings views.

## Features

- Arduino Pro Mini firmware target
- DS3231 RTC for local timekeeping, stored in UTC
- ES100 WWVB receiver for radio synchronization
- SSD1306 OLED display
- Immediate clock display at startup
- Background WWVB sync without blocking the UI
- Compact settings flow for manual UTC time, timezone, DST, and SYNC enable
- Deterministic button handling with explicit operating and display states
- Minimal `signalclock.ino` that forwards into `signalclock_app.cpp`

## Current settings flow

Long-press `MENU` to enter settings. The settings sequence is:

- `HH:MM:SS UTC` with one active field blinking
- `UTC±offset`
- `DST ON/OFF`
- `SYNC ON/OFF`

Behavior summary:

- `MENU` short advances within the current settings flow
- `MENU` long while editing UTC time commits the pending UTC time and advances to timezone
- `UP/DOWN` only act inside settings
- When `SYNC` is `OFF`, manual UTC time entry can write the RTC
- When `SYNC` is `ON`, manual UTC time entry is displayed and editable but is not committed to the RTC
- No new sync starts while settings are active
- A sync already in progress is allowed to finish while settings are active

## Platform and build

- **Board:** Arduino Pro Mini / ATmega328P (3.3V, 8 MHz)
- **Serial Monitor baud:** 9600

## Development notes

- Keep ISRs short and deterministic.
- Avoid `String` on AVR.
- Prefer non-blocking foreground logic.
- Update `README.md`, `TEST_PLAN.md`, and `DESIGN.md` with each architectural patch.

## Sketch structure

The `.ino` file is intentionally kept minimal and only forwards Arduino entry points to `signalclock_app.cpp`. This makes static analysis and future maintenance easier by keeping the application logic in normal C++ translation units.

## ES100 driver API

The ES100 driver public API is intentionally kept minimal and now exposes only:

- `es100_init()`
- `es100_start_receive()`
- `es100_service()`

Low-level inspection helpers remain file-local inside `es100.cpp`.
