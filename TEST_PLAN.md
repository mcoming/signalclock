# TEST_PLAN.md

## Scope

This test plan covers the patch that refactors the sketch into a minimal `.ino` wrapper and preserves existing runtime behavior. It also covers the existing display-state behavior, specifically that the patch:

- expands `DisplayState`
- renders explicit display views
- keeps `UP` and `DOWN` inert outside settings
- supports `DISPLAY_CLOCK`
- supports `DISPLAY_STATUS`
- supports `DISPLAY_SET_TIMEZONE`
- keeps settings flow for DST and Exit

## 1. Pre-test setup

### Hardware

Confirm button wiring:

- `MENU` -> `A0` to GND
- `UP` -> `A1` to GND
- `DOWN` -> `A2` to GND
- `SYNC` -> `A3` to GND

All buttons use `INPUT_PULLUP` and pressed = LOW.

### Software

- Build and upload the patch.
- Open Serial Monitor at 9600 baud.

## 2. Smoke test after boot

Expected:

- display initializes
- RTC time is shown immediately
- no reboot loop
- no frozen display
- WWVB background sync can start without blocking the display

Pass if:

- display is alive
- Serial output is readable
- no lockups occur

## 3. Normal display-state navigation

### CLOCK view default

On boot, expected display is `DISPLAY_CLOCK`.

Verify:

- analog clock is shown on the left
- digital time/date is shown on the right
- status line is visible

### MENU short toggles to STATUS

Press `MENU` briefly.

Expected:

- Serial prints `DISPLAY status`
- display changes to the status view

### MENU short toggles back to CLOCK

Press `MENU` briefly again.

Expected:

- Serial prints `DISPLAY clock`
- display returns to the clock view

Pass if:

- the display visibly changes between two distinct views
- repeated MENU short presses continue to toggle correctly

## 4. STATUS view rendering

From the status view, verify the display shows:

- title or obvious status layout
- current time
- operating state
- RTC state
- WWVB state
- timezone and DST information

Pass if:

- status view is clearly different from the normal clock view
- displayed status changes appropriately if sync becomes active or completes

## 5. UP and DOWN outside settings

From both `DISPLAY_CLOCK` and `DISPLAY_STATUS`:

- press `UP` short
- press `DOWN` short

Expected:

- Serial reports ignored outside settings
- no display-state change
- no setting value changes

Pass if:

- UP and DOWN are inert outside settings

## 6. Manual sync button

### Sync while idle

Press `SYNC` short when not already syncing.

Expected:

- Serial prints `SYNC requested`
- WWVB sync starts
- operating state becomes syncing
- current display view remains usable

### Sync while already syncing

Press `SYNC` short during active sync.

Expected:

- Serial prints `WWVB busy`
- sync is not restarted

Pass if:

- sync starts only once
- busy feedback appears on repeated presses during active sync

## 7. Enter settings and timezone view

From normal operation:

- hold `MENU`

Expected:

- Serial prints settings entry
- display changes to `DISPLAY_SET_TIMEZONE`

Verify the timezone view shows:

- clear indication that timezone is being edited
- current pending timezone value
- prompt or hint for button usage

## 8. Timezone editing behavior

While in `DISPLAY_SET_TIMEZONE`:

- `UP` short increments pending timezone
- `DOWN` short decrements pending timezone
- `MENU` short commits timezone and advances to `DISPLAY_SET_DST`
- `MENU` long cancels timezone edit and remains in timezone item

Pass if:

- value changes are visible in Serial
- timezone view remains stable while editing
- advancing moves to the next view

## 9. DST view behavior

While in `DISPLAY_SET_DST`:

- `UP` short toggles DST
- `DOWN` short toggles DST
- `MENU` short commits DST and advances to `DISPLAY_SET_EXIT`
- `MENU` long cancels DST edit

Verify:

- display is distinct from timezone view
- ON/OFF state is clear

## 10. Exit view behavior

While in `DISPLAY_SET_EXIT`:

- `MENU` short exits settings and returns to `DISPLAY_CLOCK`
- `UP` short returns to `DISPLAY_SET_DST`
- `DOWN` short returns to `DISPLAY_SET_DST`

Verify:

- Exit view is distinct from other settings views
- exit returns to normal operation

## 11. State interaction test

### Sync active while using CLOCK or STATUS

Start sync and toggle between `DISPLAY_CLOCK` and `DISPLAY_STATUS`.

Expected:

- both views still render correctly
- status text reflects sync activity
- no lockup occurs

### Enter settings while syncing

Start sync, then enter settings.

Expected:

- settings views still render
- sync can continue in background
- leaving settings returns to normal display without corruption

## 12. Stress test

For 2 to 3 minutes:

- toggle CLOCK/STATUS repeatedly with MENU short
- enter and leave timezone view repeatedly
- adjust timezone quickly with UP/DOWN
- trigger sync when idle and during sync

Pass if:

- no display freeze
- no reboot
- no broken state transitions

## 13. Suggested test log

```text
Build:
Upload:
Board setting:

[ ] Boot OK
[ ] CLOCK view renders OK
[ ] STATUS view renders OK
[ ] MENU short toggles CLOCK/STATUS
[ ] UP ignored outside settings
[ ] DOWN ignored outside settings
[ ] SYNC short while idle OK
[ ] SYNC short while syncing OK
[ ] Enter settings OK
[ ] Timezone view renders OK
[ ] Timezone increment OK
[ ] Timezone decrement OK
[ ] Timezone cancel OK
[ ] DST view renders OK
[ ] DST toggle OK
[ ] Exit view renders OK
[ ] Exit returns to CLOCK
[ ] No freezes
[ ] No resets
```
