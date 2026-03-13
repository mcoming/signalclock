# TEST_PLAN.md

## SignalClock Manual Test Procedure

This test plan is intended for the current patch that introduces:

- `OperatingState`
- `DisplayState`
- button dispatch structure
- manual sync button behavior
- timezone setting state flow

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
- Open Serial Monitor at the baud rate used by the sketch.

---

## 2. Smoke test after boot

### Expected result

On power-up:

- display initializes
- RTC time is shown
- normal clock behavior resumes
- no immediate resets or lockups
- if background sync starts automatically, the clock still remains usable

### Pass criteria

- no garbage on Serial
- no frozen display
- no repeated reboot loop

---

## 3. Button electrical sanity check

Press each button once and verify the firmware responds consistently.

### MENU short

Expected:

- while on the normal clock display, a short press should advance the placeholder display mode
- current patch reports this through Serial

Press `MENU` briefly 3 to 5 times.

Pass if:

- each short press produces one response
- no duplicate triggers from one press
- no missed presses under normal use

### MENU long

Expected:

- enters timezone setting mode
- this should be visible in Serial and internal behavior
- after entering, UP and DOWN should adjust the pending timezone value

Hold `MENU` for longer than the long-press threshold.

Pass if:

- one long press enters timezone setting
- it does not also act like repeated short presses

### UP short

On normal clock display:

- expected to switch to status display

In timezone setting mode:

- expected to increase `pending_timezone_hours`

### DOWN short

On normal clock display:

- may currently be unused

In timezone setting mode:

- expected to decrease `pending_timezone_hours`

### SYNC short

Expected:

- if idle, starts WWVB sync
- if already syncing, does not restart; should report busy feedback

---

## 4. Debounce behavior test

This is the most important part for the new button structure.

### Short press test

For each button:

- press briefly and release
- do this 10 times at a comfortable human speed

Pass if each intended action happens once per press.

Failure signs:

- one press triggers two actions
- a short press triggers long-press behavior
- release-only behavior appears without intended action
- very light taps are ignored too often

### Long press test

For `MENU`:

- press and hold past the long-press threshold
- repeat 5 times

Pass if:

- long press consistently enters timezone setting
- it does not also perform the short-press action first unless that is explicitly intended

### Bounce/noise test

Try these deliberately:

- very quick taps
- slightly shaky presses
- repeated presses in quick succession

Pass if:

- no obvious extra triggers
- no stuck state
- no random mode changes

---

## 5. Timezone setting flow test

This verifies the new state hierarchy behavior.

### Enter timezone setting

From normal clock display:

- hold `MENU`

Expected:

- operating state should move to `OPERATING_SETTING`
- display state should move to `DISPLAY_SET_TIMEZONE`

### Adjust timezone

While in timezone setting:

- press `UP` several times
- press `DOWN` several times

Expected:

- pending timezone value changes correctly
- no sync action is triggered
- no unrelated display-mode changes occur

### Commit timezone

While in timezone setting:

- short press `MENU`

Expected:

- pending value is committed to `timezone_hours`
- returns to normal clock display
- operating state returns to:
  - `OPERATING_SYNCING` if sync is still active
  - otherwise `OPERATING_RUNNING`

### Cancel timezone

Re-enter timezone setting, change the value, then:

- long press `MENU`

Expected:

- changes are discarded
- returns to normal clock display

Pass criteria for this section:

- enter works reliably
- UP/DOWN only affect timezone while editing
- short MENU commits
- long MENU cancels
- no stuck setting state

---

## 6. Manual sync button test

### Sync while idle

Wait until no sync is active, then:

- short press `SYNC`

Expected:

- manual WWVB sync starts
- operating state becomes `OPERATING_SYNCING`
- clock remains usable

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
- busy feedback is reported

Pass if:

- no interruption of the ongoing sync
- no crash or strange state transition

---

## 7. State interaction test

This verifies the hierarchy itself.

### Case A: sync active while normal display shown

Expected design behavior:

- clock can be syncing
- display can still be in `DISPLAY_CLOCK`

Verify:

- start sync
- ensure normal display still behaves normally

### Case B: enter timezone setting while syncing

If allowed by current patch behavior:

- start sync
- enter timezone setting with `MENU long`

Expected:

- editing works
- sync may continue in background
- leaving settings returns to syncing or running appropriately

Pass if:

- no lockup
- no corrupted UI state
- no loss of button function

---

## 8. Stress test

### Rapid user interaction

Try for 2 to 3 minutes:

- repeated MENU short presses
- enter/exit timezone setting repeatedly
- press SYNC while idle and while syncing
- alternate between UP and DOWN quickly in timezone setting

Pass if:

- no freeze
- no reboot
- no stuck button state
- no obviously lost control state

---

## 9. Long runtime test

Let the firmware run for at least:

- 15 minutes minimum
- ideally 1 hour

During that time:

- press buttons occasionally
- start at least one manual sync
- enter timezone setting at least once

Pass if:

- display remains alive
- buttons still work
- sync still works
- no gradual instability appears

---

## 10. Suggested test log template

Use something like this as you test:

```text
Build:
Upload:
Board setting:

[ ] Boot OK
[ ] RTC display OK
[ ] MENU short OK
[ ] MENU long OK
[ ] UP short OK
[ ] DOWN short OK
[ ] SYNC short while idle OK
[ ] SYNC short while syncing OK
[ ] Enter timezone setting OK
[ ] Timezone increment OK
[ ] Timezone decrement OK
[ ] Timezone commit OK
[ ] Timezone cancel OK
[ ] No duplicate button triggers
[ ] No freezes
[ ] No resets
[ ] 15+ minute runtime OK

Notes:
```

---

## 11. Highest-value failure observations to report back

If anything fails, the most useful details to report are:

- which button
- short or long press
- what state you were in
- what happened
- what you expected
- whether sync was active at the time

Example:

```text
MENU short while in DISPLAY_CLOCK triggered two increments.
SYNC short during active sync restarted the sync instead of showing busy.
UP short in timezone setting changed display mode instead of timezone.
```

---

## 12. Recommended order to run the tests

Run them in this order:

1. boot smoke test
2. individual button short presses
3. MENU long press
4. timezone setting flow
5. manual sync while idle
6. manual sync while busy
7. stress test
8. long runtime test

That order isolates problems efficiently.
