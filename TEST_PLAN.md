# TEST_PLAN.md

## SignalClock Manual Test Procedure

This test plan is intended for the current patch that introduces:

- `OperatingState`
- `DisplayState`
- `SettingItem`
- manual sync button behavior
- per-item settings commit on `MENU` short press
- per-item cancel on `MENU` long press
- explicit `Exit` settings item

---

## 1. Pre-test setup

### Hardware

Confirm the current assumed button wiring:

- `MENU` -> `A0` to GND
- `UP` -> `A1` to GND
- `DOWN` -> `A2` to GND
- `SYNC` -> `A3` to GND

All buttons should be wired as:

- pin configured `INPUT_PULLUP`
- pressed = `LOW`

### Software

- Build and upload the patched branch.
- Open Serial Monitor at 9600 baud.
- Use the Serial Monitor as the primary verification tool for this patch.

---

## 2. Expected behavioral model

### Normal clock display

- `MENU` short -> cycle display mode
- `MENU` long -> enter settings
- `UP` short -> switch to status display
- `DOWN` short -> return to clock display
- `SYNC` short -> start WWVB sync if idle, otherwise report busy

### Settings model

There are three settings items:

1. `Timezone`
2. `DST`
3. `Exit`

Behavior inside settings:

- `UP/DOWN` modify the current item
- `MENU` short -> commit current item and advance to the next item
- `MENU` long -> cancel the current item change
- `SYNC` short -> ignored while setting

### Item-specific behavior

#### Timezone
- `UP` short increments pending timezone
- `DOWN` short decrements pending timezone
- `MENU` short commits timezone and advances to DST
- `MENU` long cancels timezone edits

#### DST
- `UP` short toggles DST
- `DOWN` short toggles DST
- `MENU` short commits DST and advances to Exit
- `MENU` long cancels DST edits

#### Exit
- `MENU` short exits settings
- `MENU` long cancels Exit item only
- `UP/DOWN` short goes back to DST

---

## 3. Smoke test after boot

### Expected result

On power-up:

- display initializes
- RTC time is shown
- normal clock behavior resumes
- no immediate resets or lockups
- if background sync starts automatically, the clock still remains usable
- Serial Monitor shows an initial state snapshot

### Pass criteria

- no garbage on Serial
- no frozen display
- no repeated reboot loop
- startup state print appears reasonable

---

## 4. Manual sync button test

### Sync while idle

Wait until no sync is active, then:

- short press `SYNC`

Expected Serial output should include a sync start request and WWVB start message.

Pass if:

- sync starts once
- no lockup
- no restart loop

### Sync while already syncing

During an active WWVB sync:

- short press `SYNC`

Expected:

- current sync continues
- no restart of the sync process
- busy feedback is printed

Pass if:

- no interruption of the ongoing sync
- no crash or strange state transition

---

## 5. Enter settings

From normal clock display:

- hold `MENU`

Expected:

- operating state moves to `OPERATING_SETTING`
- display state moves to `DISPLAY_SETTINGS`
- current item becomes `SETTING_TIMEZONE`
- Serial shows settings entry and state snapshot

Pass if:

- one long press enters settings reliably
- no short-press display mode action occurs first

---

## 6. Timezone setting test

### Adjust timezone

While in `SETTING_TIMEZONE`:

- press `UP` several times
- press `DOWN` several times

Expected:

- pending timezone changes are printed
- committed timezone does not change yet

### Cancel timezone

- change pending timezone
- long press `MENU`

Expected:

- pending timezone resets to committed timezone
- still remains in settings
- current item remains timezone

### Commit timezone

- change pending timezone
- short press `MENU`

Expected:

- committed timezone is updated
- current item advances to `SETTING_DST`
- Serial prints the commit and state snapshot

Pass criteria:

- cancel does not exit settings
- commit advances to DST
- only short MENU advances

---

## 7. DST setting test

### Toggle DST

While in `SETTING_DST`:

- short press `UP`
- short press `DOWN`

Expected:

- either button toggles pending DST
- changes are shown in Serial

### Cancel DST

- toggle DST
- long press `MENU`

Expected:

- pending DST resets to committed DST
- still remains on DST item

### Commit DST

- toggle DST to desired value
- short press `MENU`

Expected:

- committed DST is updated
- current item advances to `SETTING_EXIT`

Pass criteria:

- both UP and DOWN can toggle DST
- long press cancels only the current item
- short press commits and advances

---

## 8. Exit item test

While in `SETTING_EXIT`:

### Back up from Exit

- short press `UP` or `DOWN`

Expected:

- current item returns to `SETTING_DST`

### Exit setup

- short press `MENU`

Expected:

- settings mode exits
- display state returns to `DISPLAY_CLOCK`
- operating state returns to:
  - `OPERATING_SYNCING` if sync is still active
  - otherwise `OPERATING_RUNNING`

### Long press on Exit

- long press `MENU`

Expected:

- only Exit item cancellation behavior is reported
- no exit unless explicitly committed with short MENU

Pass criteria:

- exit requires explicit Exit item + MENU short
- UP/DOWN from Exit returns to DST

---

## 9. State interaction test

### Case A: sync active while normal display shown

Expected design behavior:

- clock can be syncing
- display can still be in `DISPLAY_CLOCK`

Verify:

- start sync
- ensure normal display still behaves normally

### Case B: enter settings while syncing

- start sync
- enter settings with `MENU` long

Expected:

- editing works
- sync may continue in background
- leaving settings returns to syncing or running appropriately

Pass if:

- no lockup
- no corrupted state
- no loss of button function

---

## 10. Stress test

Try for 2 to 3 minutes:

- repeated MENU short presses on clock display
- enter/exit settings repeatedly
- cancel timezone and DST repeatedly
- press SYNC while idle and while syncing
- alternate UP and DOWN quickly in timezone setting

Pass if:

- no freeze
- no reboot
- no stuck button state
- no obviously lost control state

---

## 11. Long runtime test

Let the firmware run for at least:

- 15 minutes minimum
- ideally 1 hour

During that time:

- press buttons occasionally
- start at least one manual sync
- enter settings at least once
- commit timezone and DST at least once

Pass if:

- display remains alive
- buttons still work
- sync still works
- no gradual instability appears

---

## 12. Suggested test log template

Use something like this as you test:

```text
Build:
Upload:
Board setting:

[ ] Boot OK
[ ] RTC display OK
[ ] MENU short cycles display mode
[ ] MENU long enters settings
[ ] SYNC short while idle OK
[ ] SYNC short while syncing reports busy
[ ] Enter settings OK
[ ] Timezone increment OK
[ ] Timezone decrement OK
[ ] Timezone cancel OK
[ ] Timezone commit advances to DST
[ ] DST toggle with UP OK
[ ] DST toggle with DOWN OK
[ ] DST cancel OK
[ ] DST commit advances to Exit
[ ] Exit item back-navigation OK
[ ] Exit item commit exits settings
[ ] No duplicate button triggers
[ ] No freezes
[ ] No resets
[ ] 15+ minute runtime OK

Notes:
```

---

## 13. Highest-value failure observations to report back

If anything fails, the most useful details to report are:

- which button
- short or long press
- which setting item was active
- what happened
- what you expected
- whether sync was active at the time

Example:

```text
MENU short on SETTING_TIMEZONE exited settings instead of advancing to DST.
MENU long on SETTING_DST exited settings instead of canceling DST changes.
SYNC short while active restarted WWVB sync instead of reporting busy.
UP short on SETTING_EXIT did not return to DST.
```
