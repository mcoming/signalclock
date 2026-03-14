#ifndef ES100_H
#define ES100_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// -----------------------------------------------------------------------------
// Hardware configuration
// -----------------------------------------------------------------------------
constexpr uint8_t ES100_IRQ_PIN = 3;   // Arduino D3 = INT1 on ATmega328P
constexpr uint8_t ES100_EN_PIN  = 4;   // Adjust only if your wiring differs

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------
constexpr uint32_t ES100_DEFAULT_TIMEOUT_MS = 180000UL;  // 3 minutes

// -----------------------------------------------------------------------------
// IRQ status values
// -----------------------------------------------------------------------------
constexpr uint8_t ES100_IRQ_STATUS_RX_COMPLETE = 0x01;

// -----------------------------------------------------------------------------
// Driver state
// -----------------------------------------------------------------------------
enum Es100State : uint8_t {
  ES100_STATE_IDLE = 0,
  ES100_STATE_WAIT_IRQ,
  ES100_STATE_IRQ_PENDING,
  ES100_STATE_READING,
  ES100_STATE_DONE,
  ES100_STATE_FAILED,
  ES100_STATE_TIMEOUT
};

// -----------------------------------------------------------------------------
// Result codes
// -----------------------------------------------------------------------------
enum Es100Result : uint8_t {
  ES100_RESULT_NONE = 0,
  ES100_RESULT_SUCCESS,
  ES100_RESULT_FAILED,
  ES100_RESULT_TIMEOUT
};

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
void es100_init(TwoWire& wirePort = Wire);
bool es100_start_receive(uint32_t timeout_ms = ES100_DEFAULT_TIMEOUT_MS);
Es100Result es100_service(DateTime& out_dt);

#endif  // ES100_H
