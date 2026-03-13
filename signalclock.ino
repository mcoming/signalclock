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
RTC_DS3231 rtc;

static bool rtc_set = false;
static bool wwvb_active = false;
static bool wwvb_done = false;
static bool wwvb_failed = false;
static bool wwvb_timed_out = false;

static DateTime prev_rtc_now(2000, 1, 1, 0, 0, 0);

static uint32_t wwvb_wait_start_ms = 0;
static uint32_t last_display_refresh_ms = 0;

static const uint32_t WWVB_TIMEOUT_MS = 180000UL; // 3 minutes

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
    Serial.println(F("WWVB background sync started"));
  } else {
    wwvb_active = false;
    wwvb_failed = true;
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
    wwvb_active = false;
    wwvb_done = true;
    wwvb_failed = false;
    wwvb_timed_out = false;

    Serial.print(F("WWVB sync complete in ms: "));
    Serial.println(millis() - wwvb_wait_start_ms);
  } else if (result == ES100_RESULT_TIMEOUT) {
    wwvb_active = false;
    wwvb_done = true;
    wwvb_timed_out = true;
    wwvb_failed = false;

    Serial.print(F("WWVB timeout after ms: "));
    Serial.println(millis() - wwvb_wait_start_ms);
  } else if (result == ES100_RESULT_FAILED) {
    wwvb_active = false;
    wwvb_done = true;
    wwvb_failed = true;
    wwvb_timed_out = false;

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


static void service_button_debug(void) {
  const struct {
    ButtonId id;
    const __FlashStringHelper *name;
  } buttons[] = {
      {BUTTON_MENU, F("MENU")},
      {BUTTON_UP,   F("UP")},
      {BUTTON_DOWN, F("DOWN")},
  };

  for (uint8_t i = 0; i < (sizeof(buttons) / sizeof(buttons[0])); ++i) {
    const uint8_t events = debounce_take_events(buttons[i].id);
    if (events == BUTTON_EVENT_NONE) {
      continue;
    }

    if (events & BUTTON_EVENT_SHORT) {
      Serial.print(buttons[i].name);
      Serial.println(F("_SHORT"));
    }
    if (events & BUTTON_EVENT_LONG) {
      Serial.print(buttons[i].name);
      Serial.println(F("_LONG"));
    }
    if (events & BUTTON_EVENT_RELEASE) {
      Serial.print(buttons[i].name);
      Serial.println(F("_RELEASE"));
    }
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

  start_wwvb_background_sync();

  display_handler(prev_rtc_now, rtc_set, wwvb_active, wwvb_done, wwvb_failed,
                  wwvb_timed_out, wwvb_elapsed_seconds());
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------
void loop() {
  service_button_debug();
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
