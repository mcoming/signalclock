# TEST_PLAN.md

## Scope

This test plan covers the compact settings-flow patch that adds manual UTC time entry, timezone editing, DST editing, and SYNC enable editing while preserving the existing background WWVB behavior.

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
- WWVB background sync starts only if SYNC defaults to ON

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

- current time
- operating state
- RTC state
- WWVB state
- timezone and DST information
- SYNC ON/OFF state

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

### Sync while idle and enabled

Press `SYNC` short when not already syncing and `SYNC` setting is ON.

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

### Sync while disabled

Disable sync in settings, return to clock, then press `SYNC` short.

Expected:

- Serial prints that sync is ignored because SYNC is OFF
- WWVB sync does not start

## 7. Enter settings and UTC time view

From normal operation:

- hold `MENU`

Expected:

- Serial prints settings entry
- display changes to `DISPLAY_SET_UTC_TIME`
- the screen shows `HH:MM:SS UTC`
- only the hour field blinks initially

## 8. UTC editing behavior

While in the UTC editor:

- `UP/DOWN` wrap-adjust the active field
- `MENU` short advances `HH -> MM -> SS -> HH`
- the displayed pending UTC value changes only when `UP` or `DOWN` is pressed
- the displayed pending UTC value does not continue tracking the live RTC seconds

Pass if:

- active-field blink is obvious
- wraparound works for hour, minute, and second
- short MENU only advances the field selection

## 9. UTC commit policy

### Manual UTC commit with SYNC OFF

- enter settings
- advance to `SYNC`
- set `SYNC OFF`
- exit settings
- re-enter settings
- change UTC time
- long-press `MENU` while editing HH, MM, or SS

Expected:

- Serial prints the UTC commit
- RTC is updated
- settings advance to timezone

### Manual UTC commit with SYNC ON

- enter settings with `SYNC ON`
- change UTC time
- long-press `MENU` while editing HH, MM, or SS

Expected:

- Serial reports that UTC commit is skipped because SYNC is ON
- settings still advance to timezone
- RTC is not overwritten by the manual edit

## 10. Timezone behavior

While in `DISPLAY_SET_TIMEZONE`:

- `UP` short increments timezone with wrap `+14 -> -12`
- `DOWN` short decrements timezone with wrap `-12 -> +14`
- `MENU` short commits timezone and advances to DST

Pass if:

- wrap behavior is correct
- timezone change is visible after exiting settings

## 11. DST behavior

While in `DISPLAY_SET_DST`:

- `UP` short toggles DST
- `DOWN` short toggles DST
- `MENU` short commits DST and advances to SYNC

Verify:

- ON/OFF state is clear
- only the active value blinks

## 12. SYNC behavior in settings

While in `DISPLAY_SET_SYNC`:

- `UP` short toggles SYNC
- `DOWN` short toggles SYNC
- `MENU` short commits SYNC and exits settings to `DISPLAY_CLOCK`

Pass if:

- SYNC setting persists after leaving settings for the current runtime session
- normal clock view is shown after exit

## 13. State interaction test

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
- no new sync starts while in settings
- the active sync is allowed to finish
- if sync completes, RTC updates without changing the visible pending edit fields
- leaving settings returns to normal display without corruption

## 14. Stress test

For 2 to 3 minutes:

- toggle CLOCK/STATUS repeatedly with MENU short
- enter and leave settings repeatedly
- cycle HH/MM/SS repeatedly with MENU short
- adjust timezone quickly with UP/DOWN
- toggle DST and SYNC repeatedly
- trigger sync when idle and during sync

Pass if:

- no display freeze
- no reboot
- no broken state transitions
- blink remains stable while editing

## 15. Suggested test log

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
[ ] SYNC short while disabled blocked
[ ] Enter settings OK
[ ] UTC edit screen renders OK
[ ] UTC hour wrap OK
[ ] UTC minute wrap OK
[ ] UTC second wrap OK
[ ] UTC commit with SYNC OFF OK
[ ] UTC commit with SYNC ON blocked
[ ] Timezone wrap OK
[ ] DST toggle OK
[ ] SYNC toggle OK
[ ] Exit from SYNC returns to clock OK
[ ] Sync completion during settings preserves visible edit values
```
