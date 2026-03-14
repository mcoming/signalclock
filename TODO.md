# TODO.md

## Purpose

This file captures planned features, refactors, and validation work for **SignalClock**.

The goal is to keep a lightweight engineering backlog that is specific enough to guide future patches without turning into a full project-management system.

Each item should include:

- a short feature name
- a brief description of the user-facing need
- a proposed implementation approach
- optional notes about dependencies, risks, or test implications

---

## Suggested status tags

Use one of these tags in front of each item as needed:

- `[idea]` not yet designed in detail
- `[planned]` agreed direction, not yet implemented
- `[active]` currently being worked
- `[blocked]` waiting on another change, hardware, or testing
- `[done]` implemented and verified

---

## 1. Core usability features

### [planned] Manual time setting

**Description**  
Allow the user to manually set the clock time when WWVB has not synced and the DS3231 time is invalid or untrusted.

**Why**  
Without manual time setting, the clock can become unusable if:
- WWVB reception is unavailable
- the RTC battery fails or is removed
- the RTC contains an invalid time

This also makes bench testing easier than physically removing the DS3231 battery.

**Proposed implementation**  
- Keep the RTC stored in **UTC**
- Add a manual UTC time-setting flow in the settings UI
- Use explicit editable fields:
  - year
  - month
  - day
  - hour
  - minute
  - second
  - `SET NOW`
- `UP/DOWN` adjust the current field
- `MENU short` commits the current field and advances
- `MENU long` cancels the current field edit
- `SET NOW` writes the completed UTC timestamp to the DS3231 in one action

**Notes**  
- Display should continue to show local time by applying timezone/DST offset to the UTC RTC value
- This should integrate cleanly with the existing settings model

---

### [planned] RTC invalid / untrusted time handling

**Description**  
Detect and communicate when the RTC time is invalid or untrusted.

**Why**  
The clock should not silently present a bad time as valid.

**Proposed implementation**  
- Add an internal RTC-valid flag or trust state
- Mark the time untrusted when:
  - RTC lost power
  - RTC contents fail validity checks
- Show visible status such as `RTC invalid` or similar
- Allow recovery by:
  - WWVB sync
  - manual time set

**Notes**  
- This feature pairs naturally with manual time setting
- TEST_PLAN should include invalid-RTC recovery scenarios

---

## 2. Time sync and WWVB features

### [planned] Store time of last successful sync

**Description**  
Remember the most recent successful WWVB sync time.

**Why**  
Useful for diagnostics and status reporting.

**Proposed implementation**  
- Store the last successful sync timestamp in UTC
- Update only on successful ES100-based synchronization
- Expose it in status view or diagnostics later if desired

**Notes**  
- Keep storage lightweight
- Avoid adding unnecessary formatting or debug overhead in the core path

---

### [idea] Nighttime sync scheduling

**Description**  
Prefer scheduled WWVB sync attempts during nighttime hours when reception is typically better.

**Why**  
WWVB reception often improves at night.

**Proposed implementation**  
- Use UTC stored in RTC as the source time
- Derive local time from timezone and DST settings
- Schedule one or more sync attempts during a preferred nighttime window
- Keep manual sync available at any time

**Notes**  
- This feature depends on timezone and DST being configured
- A future refinement could distinguish first-sync behavior from maintenance-sync behavior

---

### [idea] Sync diagnostics / status improvements

**Description**  
Provide clearer user-visible sync state feedback.

**Why**  
Helpful during bring-up and troubleshooting.

**Proposed implementation**  
Possible additions:
- show `idle`, `busy`, `success`, `fail`, `timeout`
- show elapsed sync time
- optionally show time since last successful sync

**Notes**  
- Be mindful of flash usage
- Keep the status view compact

---

## 3. Settings and UI features

### [planned] Expand settings menu structure

**Description**  
Continue evolving the settings UI into a clear, linear menu.

**Why**  
The current system already supports timezone and DST editing; it should be extended in a consistent way.

**Proposed implementation**  
Maintain explicit display states for settings pages, for example:
- `DISPLAY_SET_TIMEZONE`
- `DISPLAY_SET_DST`
- `DISPLAY_SET_TIME`
- `DISPLAY_SET_EXIT`

**Notes**  
- Keep UI deterministic
- Prefer explicit commit/cancel behavior over hidden side effects

---

### [idea] Additional display formats

**Description**  
Support alternate normal-display layouts.

**Why**  
The user has already discussed toggling between different display formats.

**Proposed implementation**  
Possible future options:
- alternate clock/date layouts
- more compact status presentation
- digital-only view
- larger time emphasis view

**Notes**  
- Use a clear distinction between:
  - `DisplayState` = which page is shown
  - display format mode = how a page is rendered
- Watch flash growth carefully

---

### [idea] Show both UTC and local time in status view

**Description**  
Allow the status view to show both UTC and local display time.

**Why**  
Useful for debugging timezone and DST logic.

**Proposed implementation**  
- Keep RTC in UTC
- Derive local display time on demand
- Show both in the status page if space allows

**Notes**  
- Only worthwhile if the layout remains readable

---

## 4. Verification and maintainability

### [planned] Continue keeping README / DESIGN / TEST_PLAN in sync

**Description**  
Update the main documentation files with each architectural patch.

**Why**  
This has already been agreed as a project workflow rule.

**Proposed implementation**  
For each patch, review whether changes are needed in:
- `README.md`
- `DESIGN.md`
- `TEST_PLAN.md`

**Notes**  
- Keep docs concise but accurate
- Design discussion belongs in docs more than in source comments

---

### [idea] Code-size reduction pass

**Description**  
Reduce flash use while preserving behavior.

**Why**  
Program flash is now a constrained resource.

**Proposed implementation**  
High-priority candidates discussed so far:
- replace textual date with numeric date
- remove low-value or redundant strings
- simplify status messages
- remove dead code and stale symbols
- consider simplifying analog rendering only if necessary

**Notes**  
- Re-measure flash after every size-focused patch
- Prefer low-risk savings first

---

### [idea] A/B sync regression test workflow

**Description**  
Define a repeatable way to compare sync behavior before and after refactors.

**Why**  
Useful when cleaning internal APIs or refactoring sync code.

**Proposed implementation**  
- compare current branch vs known-good commit
- test under similar reception conditions
- record:
  - startup behavior
  - elapsed time to sync
  - timeout behavior
  - success / failure outcome

**Notes**  
- Could be documented in `TEST_PLAN.md`

---

## 5. Parking lot

Use this section for ideas that are interesting but not yet ready for design.

### [idea] Automatic DST rules instead of manual DST toggle

**Description**  
Automatically determine DST from configured rules instead of requiring manual on/off.

**Proposed implementation**  
- derive DST from calendar rules for the configured region
- likely requires deciding what regional rule set(s) to support

**Notes**  
- This is more complex than the current manual DST approach
- Only worth doing if the supported scope is clear

---

### [idea] Additional sync sources

**Description**  
Support future alternate time sources if the hardware evolves.

**Proposed implementation**  
Could include:
- GPS
- NTP
- other reference sources

**Notes**  
- Out of scope for the current AVR/ES100-only build
- Keep architecture flexible where practical
