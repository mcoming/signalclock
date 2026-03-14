#include "signalclock_app.h"

#include <Arduino.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <Wire.h>

#include "debounce.h"
#include "display.h"
#include "es100.h"

// -----------------------------------------------------------------------------
// State types
// -----------------------------------------------------------------------------
enum OperatingState : uint8_t {
  OPERATING_RUNNING = 0,
  OPERATING_SYNCING,
  OPERATING_SETTING
};

enum DisplayState : uint8_t {
  DISPLAY_CLOCK = 0,
  DISPLAY_STATUS,
  DISPLAY_SET_TIMEZONE,
  DISPLAY_SET_DST,
  DISPLAY_SET_EXIT
};

enum SettingItem : uint8_t {
  SETTING_TIMEZONE = 0,
  SETTING_DST,
  SETTING_EXIT,
  SETTING_COUNT
};

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static RTC_DS3231 rtc;

static bool rtc_set = false;
static bool wwvb_active = false;
static bool wwvb_done = false;
static bool wwvb_failed = false;
static bool wwvb_timed_out = false;

static DateTime prev_rtc_now(2000, 1, 1, 0, 0, 0);
static DateTime last_successful_sync_utc(2000, 1, 1, 0, 0, 0);
static bool last_successful_sync_valid = false;

static uint32_t wwvb_wait_start_ms = 0;
static uint32_t last_display_refresh_ms = 0;

static const uint32_t WWVB_TIMEOUT_MS = 180000UL;  // 3 minutes

static OperatingState operating_state = OPERATING_RUNNING;
static DisplayState display_state = DISPLAY_CLOCK;
static SettingItem current_setting_item = SETTING_TIMEZONE;

static int8_t timezone_hours = 0;
static bool dst_enabled = false;

static int8_t pending_timezone_hours = 0;
static bool pending_dst_enabled = false;

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------
static const __FlashStringHelper *operating_state_name(OperatingState state) {
  switch (state) {
    case OPERATING_RUNNING:
      return F("RUNNING");
    case OPERATING_SYNCING:
      return F("SYNCING");
    case OPERATING_SETTING:
      return F("SETTING");
    default:
      return F("UNKNOWN");
  }
}

static const __FlashStringHelper *display_state_name(DisplayState state) {
  switch (state) {
    case DISPLAY_CLOCK:
      return F("CLOCK");
    case DISPLAY_STATUS:
      return F("STATUS");
    case DISPLAY_SET_TIMEZONE:
      return F("SETTING TIMEZONE");
    case DISPLAY_SET_DST:
      return F("SETTING DST");
    case DISPLAY_SET_EXIT:
      return F("SETTING EXIT");
    default:
      return F("UNKNOWN");
  }
}

static const __FlashStringHelper *setting_item_name(SettingItem item) {
  switch (item) {
    case SETTING_TIMEZONE:
      return F("TIMEZONE");
    case SETTING_DST:
      return F("DST");
    case SETTING_EXIT:
      return F("EXIT");
    default:
      return F("UNKNOWN");
  }
}

static void print_state_summary(void) {
  Serial.print(F("STATE op="));
  Serial.print(operating_state_name(operating_state));
  Serial.print(F(" display="));
  Serial.print(display_state_name(display_state));
  Serial.print(F(" item="));
  Serial.print(setting_item_name(current_setting_item));
  Serial.print(F(" tz="));
  Serial.print(timezone_hours);
  Serial.print(F(" pending_tz="));
  Serial.print(pending_timezone_hours);
  Serial.print(F(" dst="));
  Serial.print(dst_enabled ? F("ON") : F("OFF"));
  Serial.print(F(" pending_dst="));
  Serial.println(pending_dst_enabled ? F("ON") : F("OFF"));
}

static void fatal_blink_forever(uint16_t period_ms) {
  pinMode(LED_BUILTIN, OUTPUT);

  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(period_ms / 2);
    digitalWrite(LED_BUILTIN, LOW);
    delay(period_ms / 2);
  }
}

static void setup_rtc_local(void) {
  if (!rtc.begin()) {
    display_msg(F("RTC not found."), 0, 32, SSD1306_WHITE, SSD1306_BLACK, 1);
    fatal_blink_forever(4000);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    display_msg(F("RTC lost power."), 0, 32, SSD1306_WHITE, SSD1306_BLACK, 1);
    delay(1000);
  } else {
    display_msg(F("RTC found."), 0, 32, SSD1306_WHITE, SSD1306_BLACK, 1);
    delay(500);
  }
}

static void enter_settings(void) {
  pending_timezone_hours = timezone_hours;
  pending_dst_enabled = dst_enabled;
  operating_state = OPERATING_SETTING;
  display_state = DISPLAY_SET_TIMEZONE;
  current_setting_item = SETTING_TIMEZONE;
  Serial.println(F("SETTINGS enter"));
  print_state_summary();
}

static void exit_settings(void) {
  operating_state = wwvb_active ? OPERATING_SYNCING : OPERATING_RUNNING;
  display_state = DISPLAY_CLOCK;
  current_setting_item = SETTING_TIMEZONE;
  Serial.println(F("SETTINGS exit"));
  print_state_summary();
}

static void cancel_current_setting(void) {
  switch (current_setting_item) {
    case SETTING_TIMEZONE:
      pending_timezone_hours = timezone_hours;
      Serial.println(F("TIMEZONE canceled"));
      break;
    case SETTING_DST:
      pending_dst_enabled = dst_enabled;
      Serial.println(F("DST canceled"));
      break;
    case SETTING_EXIT:
      Serial.println(F("EXIT item has nothing to cancel"));
      break;
    default:
      break;
  }
  print_state_summary();
}

static void commit_current_setting_and_advance(void) {
  switch (current_setting_item) {
    case SETTING_TIMEZONE:
      timezone_hours = pending_timezone_hours;
      current_setting_item = SETTING_DST;
      display_state = DISPLAY_SET_DST;
      Serial.print(F("TIMEZONE committed: "));
      Serial.println(timezone_hours);
      break;
    case SETTING_DST:
      dst_enabled = pending_dst_enabled;
      current_setting_item = SETTING_EXIT;
      display_state = DISPLAY_SET_EXIT;
      Serial.print(F("DST committed: "));
      Serial.println(dst_enabled ? F("ON") : F("OFF"));
      break;
    case SETTING_EXIT:
      Serial.println(F("EXIT selected"));
      exit_settings();
      return;
    default:
      break;
  }
  print_state_summary();
}

static void move_to_previous_setting_item(void) {
  if (current_setting_item == SETTING_EXIT) {
    current_setting_item = SETTING_DST;
    display_state = DISPLAY_SET_DST;
    Serial.println(F("SETTINGS moved back to DST"));
  } else {
    Serial.println(F("No previous settings item"));
  }
  print_state_summary();
}

static void start_wwvb_background_sync(void) {
  wwvb_failed = false;
  wwvb_timed_out = false;
  wwvb_done = false;

  es100_init(Wire);

  if (es100_start_receive(WWVB_TIMEOUT_MS)) {
    wwvb_active = true;
    wwvb_wait_start_ms = millis();
    if (operating_state != OPERATING_SETTING) {
      operating_state = OPERATING_SYNCING;
    }
    Serial.println(F("WWVB background sync started"));
  } else {
    wwvb_active = false;
    wwvb_failed = true;
    if (operating_state != OPERATING_SETTING) {
      operating_state = OPERATING_RUNNING;
    }
    Serial.println(F("WWVB start failed"));
  }
}

static void request_manual_sync(void) {
  if (!wwvb_active) {
    Serial.println(F("SYNC requested"));
    start_wwvb_background_sync();
  } else {
    Serial.println(F("WWVB busy"));
  }
}

static void service_wwvb_background_sync(void) {
  if (!wwvb_active || wwvb_done) {
    return;
  }

  DateTime wwvb_dt;
  Es100Result result = es100_service(wwvb_dt);

  if (result == ES100_RESULT_SUCCESS) {
    rtc.adjust(wwvb_dt);
    last_successful_sync_utc = wwvb_dt;
    last_successful_sync_valid = true;

    rtc_set = true;
    wwvb_active = false;
    wwvb_done = true;
    wwvb_failed = false;
    wwvb_timed_out = false;
    if (operating_state != OPERATING_SETTING) {
      operating_state = OPERATING_RUNNING;
    }

    Serial.print(F("WWVB sync complete in ms: "));
    Serial.println(millis() - wwvb_wait_start_ms);
  } else if (result == ES100_RESULT_TIMEOUT) {
    wwvb_active = false;
    wwvb_done = true;
    wwvb_timed_out = true;
    wwvb_failed = false;
    if (operating_state != OPERATING_SETTING) {
      operating_state = OPERATING_RUNNING;
    }

    Serial.print(F("WWVB timeout after ms: "));
    Serial.println(millis() - wwvb_wait_start_ms);
  } else if (result == ES100_RESULT_FAILED) {
    wwvb_active = false;
    wwvb_done = true;
    wwvb_failed = true;
    wwvb_timed_out = false;
    if (operating_state != OPERATING_SETTING) {
      operating_state = OPERATING_RUNNING;
    }

    Serial.println(F("WWVB failed"));
  }
}

static uint16_t wwvb_elapsed_seconds(void) {
  if (!wwvb_active) {
    return 0;
  }

  return (uint16_t)((millis() - wwvb_wait_start_ms) / 1000UL);
}

static void process_running_button(ButtonId id, uint8_t events) {
  if ((id == BUTTON_SYNC) && (events & BUTTON_EVENT_SHORT)) {
    request_manual_sync();
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_SHORT)) {
    if (display_state == DISPLAY_CLOCK) {
      display_state = DISPLAY_STATUS;
      Serial.println(F("DISPLAY status"));
    } else {
      display_state = DISPLAY_CLOCK;
      Serial.println(F("DISPLAY clock"));
    }
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_LONG)) {
    enter_settings();
    return;
  }

  if ((id == BUTTON_UP) && (events & BUTTON_EVENT_SHORT)) {
    Serial.println(F("UP ignored outside settings"));
    return;
  }

  if ((id == BUTTON_DOWN) && (events & BUTTON_EVENT_SHORT)) {
    Serial.println(F("DOWN ignored outside settings"));
    return;
  }
}

static void process_setting_button(ButtonId id, uint8_t events) {
  if ((id == BUTTON_SYNC) && (events & BUTTON_EVENT_SHORT)) {
    Serial.println(F("SYNC ignored while setting"));
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_LONG)) {
    cancel_current_setting();
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_SHORT)) {
    commit_current_setting_and_advance();
    return;
  }

  if ((id == BUTTON_UP) && (events & BUTTON_EVENT_SHORT)) {
    switch (current_setting_item) {
      case SETTING_TIMEZONE:
        if (pending_timezone_hours < 14) {
          pending_timezone_hours++;
        }
        Serial.print(F("TIMEZONE pending="));
        Serial.println(pending_timezone_hours);
        break;
      case SETTING_DST:
        pending_dst_enabled = !pending_dst_enabled;
        Serial.print(F("DST pending="));
        Serial.println(pending_dst_enabled ? F("ON") : F("OFF"));
        break;
      case SETTING_EXIT:
        move_to_previous_setting_item();
        break;
      default:
        break;
    }
    return;
  }

  if ((id == BUTTON_DOWN) && (events & BUTTON_EVENT_SHORT)) {
    switch (current_setting_item) {
      case SETTING_TIMEZONE:
        if (pending_timezone_hours > -12) {
          pending_timezone_hours--;
        }
        Serial.print(F("TIMEZONE pending="));
        Serial.println(pending_timezone_hours);
        break;
      case SETTING_DST:
        pending_dst_enabled = !pending_dst_enabled;
        Serial.print(F("DST pending="));
        Serial.println(pending_dst_enabled ? F("ON") : F("OFF"));
        break;
      case SETTING_EXIT:
        move_to_previous_setting_item();
        break;
      default:
        break;
    }
    return;
  }
}

static void process_button_event(ButtonId id, uint8_t events) {
  if (events == BUTTON_EVENT_NONE) {
    return;
  }

  if (operating_state == OPERATING_SETTING) {
    process_setting_button(id, events);
  } else {
    process_running_button(id, events);
  }
}

static void service_buttons(void) {
  const ButtonId buttons[] = {BUTTON_MENU, BUTTON_UP, BUTTON_DOWN, BUTTON_SYNC};

  for (uint8_t i = 0; i < (sizeof(buttons) / sizeof(buttons[0])); ++i) {
    const uint8_t events = debounce_take_events(buttons[i]);
    process_button_event(buttons[i], events);
  }
}

static int8_t get_total_display_offset_hours(void) {
  return timezone_hours + (dst_enabled ? 1 : 0);
}

static DateTime apply_display_offset(const DateTime &utc_now) {
  return utc_now + TimeSpan((int32_t)get_total_display_offset_hours() * 3600L);
}

void signalclock_app_setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(25000, true);

  setup_display();
  debounce_init();
  setup_rtc_local();

  DateTime rtc_now_utc = rtc.now();
  prev_rtc_now = apply_display_offset(rtc_now_utc);
  rtc_set = !rtc.lostPower();

  operating_state = OPERATING_RUNNING;
  display_state = DISPLAY_CLOCK;

  start_wwvb_background_sync();

  display_handler(prev_rtc_now, rtc_set, wwvb_active, wwvb_done, wwvb_failed,
                  wwvb_timed_out, wwvb_elapsed_seconds(),
                  static_cast<uint8_t>(operating_state),
                  static_cast<uint8_t>(display_state), timezone_hours,
                  dst_enabled, pending_timezone_hours, pending_dst_enabled);
}

void signalclock_app_loop(void) {
  service_buttons();
  service_wwvb_background_sync();

  DateTime rtc_now_utc = rtc.now();
  DateTime display_now = apply_display_offset(rtc_now_utc);

  const bool second_changed = (display_now.second() != prev_rtc_now.second());
  const bool syncing_refresh_due =
      wwvb_active && ((uint32_t)(millis() - last_display_refresh_ms) >= 250UL);

  if (second_changed || syncing_refresh_due) {
    prev_rtc_now = display_now;
    last_display_refresh_ms = millis();

    display_handler(display_now, rtc_set, wwvb_active, wwvb_done, wwvb_failed,
                    wwvb_timed_out, wwvb_elapsed_seconds(),
                    static_cast<uint8_t>(operating_state),
                    static_cast<uint8_t>(display_state), timezone_hours,
                    dst_enabled, pending_timezone_hours, pending_dst_enabled);
  }
}
