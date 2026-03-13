#include <Arduino.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <Wire.h>

#include "display.h"
#include "es100.h"
#include "debounce.h"

// -----------------------------------------------------------------------------
// Globals
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
  DISPLAY_SET_DISPLAY_MODE
};

RTC_DS3231 rtc;

static bool rtc_set = false;
static bool wwvb_active = false;
static bool wwvb_done = false;
static bool wwvb_failed = false;
static bool wwvb_timed_out = false;

static OperatingState operating_state = OPERATING_RUNNING;
static DisplayState display_state = DISPLAY_CLOCK;

static int8_t timezone_hours = 0;
static uint8_t display_mode = 0;
static int8_t pending_timezone_hours = 0;
static uint8_t pending_display_mode = 0;

static DateTime prev_rtc_now(2000, 1, 1, 0, 0, 0);

static uint32_t wwvb_wait_start_ms = 0;
static uint32_t last_display_refresh_ms = 0;

static const uint32_t WWVB_TIMEOUT_MS = 180000UL; // 3 minutes

static void begin_sync(void);
static void finish_sync_success(void);
static void finish_sync_timeout(void);
static void finish_sync_failure(void);
static void enter_timezone_setting(void);
static void commit_timezone_setting(void);
static void cancel_timezone_setting(void);
static void process_clock_display_button(ButtonId id, uint8_t events);
static void process_status_display_button(ButtonId id, uint8_t events);
static void process_timezone_display_button(ButtonId id, uint8_t events);
static void process_display_mode_button(ButtonId id, uint8_t events);
static void process_button_event(ButtonId id, uint8_t events);
static void service_buttons(void);

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------
static DateTime compensate_irq_latency(const DateTime &dt, uint32_t irq_ms) {
  const uint32_t age_ms = millis() - irq_ms;

  if (age_ms >= 1000UL) {
    return dt + TimeSpan(age_ms / 1000UL);
  }

  return dt;
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
    if (operating_state == OPERATING_SYNCING) {
      operating_state = OPERATING_RUNNING;
    }
    Serial.println(F("WWVB start failed"));
  }
}

static void service_wwvb_background_sync(void) {
  if (!wwvb_active || wwvb_done) {
    return;
  }

  DateTime wwvb_dt;
  Es100Result result = es100_service(wwvb_dt);

  if (result == ES100_RESULT_SUCCESS) {
    DateTime corrected = wwvb_dt;

    if (es100_irq_seen()) {
      corrected = compensate_irq_latency(wwvb_dt, es100_get_last_irq_ms());
    }

    rtc.adjust(corrected);

    rtc_set = true;
    finish_sync_success();

    Serial.print(F("WWVB sync complete in ms: "));
    Serial.println(millis() - wwvb_wait_start_ms);
  } else if (result == ES100_RESULT_TIMEOUT) {
    finish_sync_timeout();

    Serial.print(F("WWVB timeout after ms: "));
    Serial.println(millis() - wwvb_wait_start_ms);
  } else if (result == ES100_RESULT_FAILED) {
    finish_sync_failure();

    Serial.print(F("WWVB failed, irq_status=0x"));
    Serial.println(es100_get_last_irq_status(), HEX);
  }
}

static uint16_t wwvb_elapsed_seconds(void) {
  if (!wwvb_active) {
    return 0;
  }

  return (uint16_t)((millis() - wwvb_wait_start_ms) / 1000UL);
}


static void begin_sync(void) {
  if (wwvb_active) {
    Serial.println(F("WWVB busy"));
    return;
  }

  start_wwvb_background_sync();
}

static void finish_sync_success(void) {
  wwvb_active = false;
  wwvb_done = true;
  wwvb_failed = false;
  wwvb_timed_out = false;

  if (operating_state == OPERATING_SYNCING) {
    operating_state = OPERATING_RUNNING;
  }
}

static void finish_sync_timeout(void) {
  wwvb_active = false;
  wwvb_done = true;
  wwvb_failed = false;
  wwvb_timed_out = true;

  if (operating_state == OPERATING_SYNCING) {
    operating_state = OPERATING_RUNNING;
  }
}

static void finish_sync_failure(void) {
  wwvb_active = false;
  wwvb_done = true;
  wwvb_failed = true;
  wwvb_timed_out = false;

  if (operating_state == OPERATING_SYNCING) {
    operating_state = OPERATING_RUNNING;
  }
}

static void enter_timezone_setting(void) {
  pending_timezone_hours = timezone_hours;
  operating_state = OPERATING_SETTING;
  display_state = DISPLAY_SET_TIMEZONE;
  Serial.print(F("Enter timezone setting: "));
  Serial.println(pending_timezone_hours);
}

static void commit_timezone_setting(void) {
  timezone_hours = pending_timezone_hours;
  operating_state = wwvb_active ? OPERATING_SYNCING : OPERATING_RUNNING;
  display_state = DISPLAY_CLOCK;
  Serial.print(F("Timezone set to "));
  Serial.println(timezone_hours);
}

static void cancel_timezone_setting(void) {
  operating_state = wwvb_active ? OPERATING_SYNCING : OPERATING_RUNNING;
  display_state = DISPLAY_CLOCK;
  Serial.println(F("Timezone edit canceled"));
}

static void process_clock_display_button(ButtonId id, uint8_t events) {
  if ((id == BUTTON_SYNC) && (events & BUTTON_EVENT_SHORT)) {
    begin_sync();
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_SHORT)) {
    display_mode = (uint8_t)((display_mode + 1U) % 2U);
    Serial.print(F("Display mode -> "));
    Serial.println(display_mode);
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_LONG)) {
    enter_timezone_setting();
    return;
  }

  if ((id == BUTTON_UP) && (events & BUTTON_EVENT_SHORT)) {
    display_state = DISPLAY_STATUS;
    Serial.println(F("Display -> STATUS"));
    return;
  }

  if ((id == BUTTON_DOWN) && (events & BUTTON_EVENT_SHORT)) {
    display_state = DISPLAY_CLOCK;
    return;
  }
}

static void process_status_display_button(ButtonId id, uint8_t events) {
  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_SHORT)) {
    display_state = DISPLAY_CLOCK;
    Serial.println(F("Display -> CLOCK"));
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_LONG)) {
    enter_timezone_setting();
    return;
  }

  if ((id == BUTTON_SYNC) && (events & BUTTON_EVENT_SHORT)) {
    begin_sync();
    return;
  }
}

static void process_timezone_display_button(ButtonId id, uint8_t events) {
  if ((id == BUTTON_UP) && (events & BUTTON_EVENT_SHORT)) {
    if (pending_timezone_hours < 14) {
      pending_timezone_hours++;
    }
    Serial.print(F("TZ -> "));
    Serial.println(pending_timezone_hours);
    return;
  }

  if ((id == BUTTON_DOWN) && (events & BUTTON_EVENT_SHORT)) {
    if (pending_timezone_hours > -12) {
      pending_timezone_hours--;
    }
    Serial.print(F("TZ -> "));
    Serial.println(pending_timezone_hours);
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_SHORT)) {
    commit_timezone_setting();
    return;
  }

  if ((id == BUTTON_MENU) && (events & BUTTON_EVENT_LONG)) {
    cancel_timezone_setting();
    return;
  }
}

static void process_display_mode_button(ButtonId id, uint8_t events) {
  (void)id;
  (void)events;
  display_state = DISPLAY_CLOCK;
}

static void process_button_event(ButtonId id, uint8_t events) {
  if (events == BUTTON_EVENT_NONE) {
    return;
  }

  switch (display_state) {
    case DISPLAY_CLOCK:
      process_clock_display_button(id, events);
      break;

    case DISPLAY_STATUS:
      process_status_display_button(id, events);
      break;

    case DISPLAY_SET_TIMEZONE:
      process_timezone_display_button(id, events);
      break;

    case DISPLAY_SET_DISPLAY_MODE:
      process_display_mode_button(id, events);
      break;

    default:
      display_state = DISPLAY_CLOCK;
      break;
  }
}

static void service_buttons(void) {
  const ButtonId buttons[] = {
      BUTTON_MENU,
      BUTTON_UP,
      BUTTON_DOWN,
      BUTTON_SYNC,
  };

  for (uint8_t i = 0; i < (sizeof(buttons) / sizeof(buttons[0])); ++i) {
    const uint8_t events = debounce_take_events(buttons[i]);
    process_button_event(buttons[i], events);
  }
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);

  Wire.begin();
  Wire.setClock(100000);
  // Uncomment if your AVR core supports it:
  // Wire.setWireTimeout(25000, true);

  setup_display();
  debounce_init();
  setup_rtc_local();

  // Show RTC immediately, then sync in background.
  prev_rtc_now = rtc.now();
  rtc_set = !rtc.lostPower();
  pending_timezone_hours = timezone_hours;
  pending_display_mode = display_mode;
  operating_state = OPERATING_RUNNING;
  display_state = DISPLAY_CLOCK;

  start_wwvb_background_sync();

  display_handler(prev_rtc_now, rtc_set, wwvb_active, wwvb_done, wwvb_failed,
                  wwvb_timed_out, wwvb_elapsed_seconds());
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------
void loop() {
  service_buttons();
  service_wwvb_background_sync();

  DateTime rtc_now = rtc.now();

  const bool second_changed = (rtc_now.second() != prev_rtc_now.second());
  const bool syncing_refresh_due =
      wwvb_active && ((uint32_t)(millis() - last_display_refresh_ms) >= 250UL);

  if (second_changed || syncing_refresh_due) {
    prev_rtc_now = rtc_now;
    last_display_refresh_ms = millis();

    display_handler(rtc_now, rtc_set, wwvb_active, wwvb_done, wwvb_failed,
                    wwvb_timed_out, wwvb_elapsed_seconds());
  }
}
