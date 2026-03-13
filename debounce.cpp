#include "debounce.h"

#include <avr/interrupt.h>
#include <avr/io.h>

namespace {

struct ButtonDebouncer {
  volatile uint16_t hold_ticks;
  volatile uint8_t stable_down;
  volatile uint8_t long_reported;
  volatile uint8_t events;

  uint8_t integrator;
  volatile uint8_t *pin_reg;
  uint8_t pin_mask;
};

ButtonDebouncer g_buttons[BUTTON_COUNT];

inline bool raw_pin_down(const ButtonDebouncer &b)
{
  // INPUT_PULLUP wiring: pressed = LOW
  return ((*b.pin_reg & b.pin_mask) == 0U);
}

inline void debounce_button(ButtonDebouncer &b)
{
  const bool raw_down = raw_pin_down(b);

  // Saturating debounce integrator:
  // 0 = stably released
  // DEBOUNCE_MS_THRESHOLD = stably pressed
  if (raw_down) {
    if (b.integrator < DEBOUNCE_MS_THRESHOLD) {
      b.integrator++;
    }
  } else {
    if (b.integrator > 0) {
      b.integrator--;
    }
  }

  // Stable press detected
  if ((b.integrator >= DEBOUNCE_MS_THRESHOLD) && !b.stable_down) {
    b.stable_down = 1;
    b.long_reported = 0;
    b.hold_ticks = 0;
  }

  // Stable release detected
  if ((b.integrator == 0) && b.stable_down) {
    if (!b.long_reported) {
      b.events |= BUTTON_EVENT_SHORT;
    } else {
      b.events |= BUTTON_EVENT_RELEASE;
    }
    b.stable_down = 0;
    b.long_reported = 0;
    b.hold_ticks = 0;
  }

  // Hold / long press timing
  if (b.stable_down) {
    if (b.hold_ticks < 0xFFFFU) {
      b.hold_ticks++;
    }

    if (!b.long_reported && (b.hold_ticks >= LONGPRESS_MS_THRESHOLD)) {
      b.events |= BUTTON_EVENT_LONG;
      b.long_reported = 1;
    }
  }
}

void configure_button(ButtonId id, uint8_t pin)
{
  pinMode(pin, INPUT_PULLUP);

  ButtonDebouncer &b = g_buttons[id];
  b.hold_ticks = 0;
  b.stable_down = 0;
  b.long_reported = 0;
  b.events = 0;
  b.integrator = 0;
  b.pin_reg = portInputRegister(digitalPinToPort(pin));
  b.pin_mask = digitalPinToBitMask(pin);
}

void setup_timer2_1khz(void)
{
  // Timer2 CTC at 1 kHz.
  // 8 MHz CPU: 8 MHz / 64 = 125 kHz, OCR2A = 124 -> 125 counts = 1 ms
  // 16 MHz CPU: 16 MHz / 128 = 125 kHz, OCR2A = 124 -> 125 counts = 1 ms
  noInterrupts();
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  OCR2A = 124;
  TCCR2A |= (1 << WGM21);

#ifndef F_CPU
# error F_CPU is not defined
#endif

#if (F_CPU == 8000000) || (F_CPU == 8000000UL) || (F_CPU == 8000000L)
  TCCR2B |= (1 << CS22);                  // /64
#elif (F_CPU == 16000000) || (F_CPU == 16000000UL) || (F_CPU == 16000000L)
  TCCR2B |= (1 << CS22) | (1 << CS20);    // /128
#else
# error Unsupported F_CPU for Timer2 1 kHz debounce tick
#endif


  TIMSK2 |= (1 << OCIE2A);
  interrupts();
}

}  // namespace

void debounce_init(void)
{
  configure_button(BUTTON_MENU, MENU_BTN_PIN);
  configure_button(BUTTON_UP, UP_BTN_PIN);
  configure_button(BUTTON_DOWN, DOWN_BTN_PIN);
  setup_timer2_1khz();
}

uint8_t debounce_take_events(ButtonId id)
{
  uint8_t events;
  noInterrupts();
  events = g_buttons[id].events;
  g_buttons[id].events = 0;
  interrupts();
  return events;
}

bool debounce_is_down(ButtonId id)
{
  uint8_t down;
  noInterrupts();
  down = g_buttons[id].stable_down;
  interrupts();
  return down != 0;
}

uint16_t debounce_hold_ms(ButtonId id)
{
  uint16_t hold_ms;
  noInterrupts();
  hold_ms = g_buttons[id].hold_ticks;
  interrupts();
  return hold_ms;
}

ISR(TIMER2_COMPA_vect)
{
  debounce_button(g_buttons[BUTTON_MENU]);
  debounce_button(g_buttons[BUTTON_UP]);
  debounce_button(g_buttons[BUTTON_DOWN]);
}
