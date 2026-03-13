#include "es100.h"

// -----------------------------------------------------------------------------
// Real ES100 register map and control values from the original project
// -----------------------------------------------------------------------------
namespace {

constexpr uint8_t ES100_I2C_ADDR = 0x32;
constexpr uint8_t ES100_DEVICE_ID = 0x10;

constexpr uint8_t ES100_CONTROL0_REG = 0x00;
constexpr uint8_t ES100_CONTROL1_REG = 0x01;
constexpr uint8_t ES100_IRQ_STATUS_REG = 0x02;
constexpr uint8_t ES100_STATUS0_REG = 0x03;
constexpr uint8_t ES100_YEAR_REG = 0x04;
constexpr uint8_t ES100_MONTH_REG = 0x05;
constexpr uint8_t ES100_DAY_REG = 0x06;
constexpr uint8_t ES100_HOUR_REG = 0x07;
constexpr uint8_t ES100_MINUTE_REG = 0x08;
constexpr uint8_t ES100_SECOND_REG = 0x09;
constexpr uint8_t ES100_NEXT_DST_MONTH_REG = 0x0A;
constexpr uint8_t ES100_NEXT_DST_DAY_REG = 0x0B;
constexpr uint8_t ES100_NEXT_DST_HOUR_REG = 0x0C;
constexpr uint8_t ES100_DEVICE_ID_REG = 0x0D;

// Control values from original code
constexpr uint8_t START_ANT_1 = 0x01;
constexpr uint8_t ANT_1_OFF = 0x03;
constexpr uint8_t ANT_2_OFF = 0x05;
constexpr uint8_t START_ANT_2 = 0x09;
constexpr uint8_t TRACKING_2 = 0x13;
constexpr uint8_t TRACKING_1 = 0x15;

constexpr uint16_t ENABLE_DELAY_US = 1500;

TwoWire *g_wire = &Wire;

volatile bool g_irq_pending = false;
volatile uint8_t g_irq_count = 0;
volatile uint32_t g_irq_ms = 0;

Es100State g_state = ES100_STATE_IDLE;
uint32_t g_start_ms = 0;
uint32_t g_timeout_ms = ES100_DEFAULT_TIMEOUT_MS;

uint32_t g_last_irq_ms = 0;
uint8_t g_last_irq_count = 0;
uint8_t g_last_irq_status = 0;
bool g_last_i2c_ok = true;

// -----------------------------------------------------------------------------
// ISR - keep minimal
// -----------------------------------------------------------------------------
void es100_isr() {
  g_irq_ms = millis();
  g_irq_count++;
  g_irq_pending = true;
}

// -----------------------------------------------------------------------------
// Atomic event fetch
// -----------------------------------------------------------------------------
struct Es100IrqEvent {
  bool pending;
  uint8_t count;
  uint32_t irq_ms;
};

Es100IrqEvent take_irq_event() {
  Es100IrqEvent ev;

  noInterrupts();
  ev.pending = g_irq_pending;
  ev.count = g_irq_count;
  ev.irq_ms = g_irq_ms;
  g_irq_pending = false;
  g_irq_count = 0;
  interrupts();

  return ev;
}

// -----------------------------------------------------------------------------
// Interrupt helpers
// -----------------------------------------------------------------------------
void irq_detach() {
  detachInterrupt(digitalPinToInterrupt(ES100_IRQ_PIN));
  EIFR = (1 << INTF1); // D3 = INT1
}

void irq_attach_clean() {
  // Match original intent:
  // - clear stale flags
  // - attach on falling edge after IRQ status has been read once
  EIFR = (1 << INTF0) | (1 << INTF1);
  attachInterrupt(digitalPinToInterrupt(ES100_IRQ_PIN), es100_isr, FALLING);
}

// -----------------------------------------------------------------------------
// I2C helpers
// -----------------------------------------------------------------------------
bool i2c_write_reg(uint8_t reg, uint8_t value) {
  g_wire->beginTransmission(ES100_I2C_ADDR);
  g_wire->write(reg);
  g_wire->write(value);

  const uint8_t rc = g_wire->endTransmission();
  g_last_i2c_ok = (rc == 0);
  return g_last_i2c_ok;
}

bool i2c_read_regs(uint8_t reg, uint8_t *buf, uint8_t len) {
  g_wire->beginTransmission(ES100_I2C_ADDR);
  g_wire->write(reg);

  uint8_t rc = g_wire->endTransmission(); // STOP, not repeated-start
  if (rc != 0) {
    g_last_i2c_ok = false;
    return false;
  }

  const uint8_t got =
      g_wire->requestFrom((int)ES100_I2C_ADDR, (int)len, (int)true);
  if (got != len) {
    while (g_wire->available()) {
      (void)g_wire->read();
    }
    g_last_i2c_ok = false;
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    buf[i] = g_wire->read();
  }

  g_last_i2c_ok = true;
  return true;
}

uint8_t bcd_to_bin(uint8_t x) { return (uint8_t)(x - 6 * (x >> 4)); }

// -----------------------------------------------------------------------------
// Internal helpers matching original ES100 sequence
// -----------------------------------------------------------------------------
bool es100_get_device_id_internal(uint8_t &device_id) {
  uint8_t v = 0;
  if (!i2c_read_regs(ES100_DEVICE_ID_REG, &v, 1)) {
    return false;
  }

  device_id = v;
  return true;
}

bool hw_start_receive() {

  digitalWrite(ES100_EN_PIN, LOW);
  delayMicroseconds(ENABLE_DELAY_US);

  digitalWrite(ES100_EN_PIN, HIGH);
  delayMicroseconds(ENABLE_DELAY_US);

  uint8_t device_id = 0xFF;
  if (!es100_get_device_id_internal(device_id)) {
    return false;
  }

  if (device_id != ES100_DEVICE_ID) {
    return false;
  }

  if (!i2c_write_reg(ES100_CONTROL0_REG, ANT_2_OFF)) {
    return false;
  }

  uint8_t dummy = 0;
  if (!i2c_read_regs(ES100_IRQ_STATUS_REG, &dummy, 1)) {
    return false;
  }

  return true;
}

void hw_stop() {
  // Match original behavior: just shut the module down
  digitalWrite(ES100_EN_PIN, LOW);
}

} // namespace

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
void es100_init(TwoWire &wirePort) {
  g_wire = &wirePort;

  pinMode(ES100_EN_PIN, OUTPUT);
  digitalWrite(ES100_EN_PIN, LOW);

  pinMode(ES100_IRQ_PIN, INPUT_PULLUP);

  noInterrupts();
  g_irq_pending = false;
  g_irq_count = 0;
  g_irq_ms = 0;
  interrupts();

  g_state = ES100_STATE_IDLE;
  g_start_ms = 0;
  g_timeout_ms = ES100_DEFAULT_TIMEOUT_MS;
  g_last_irq_ms = 0;
  g_last_irq_count = 0;
  g_last_irq_status = 0;
  g_last_i2c_ok = true;

  irq_detach();
}

bool es100_start_receive(uint32_t timeout_ms) {
  noInterrupts();
  g_irq_pending = false;
  g_irq_count = 0;
  g_irq_ms = 0;
  interrupts();

  g_last_irq_ms = 0;
  g_last_irq_count = 0;
  g_last_irq_status = 0;
  g_timeout_ms = timeout_ms;

  irq_detach();

  if (!hw_start_receive()) {
    g_state = ES100_STATE_FAILED;
    return false;
  }

  g_start_ms = millis();
  g_state = ES100_STATE_WAIT_IRQ;

  irq_attach_clean();
  return true;
}

Es100Result es100_service(DateTime &out_dt) {
  switch (g_state) {
  case ES100_STATE_IDLE:
  case ES100_STATE_DONE:
    return ES100_RESULT_NONE;

  case ES100_STATE_FAILED:
    return ES100_RESULT_FAILED;

  case ES100_STATE_TIMEOUT:
    return ES100_RESULT_TIMEOUT;

  case ES100_STATE_WAIT_IRQ: {
    const Es100IrqEvent ev = take_irq_event();

    if (ev.pending) {
      g_last_irq_ms = ev.irq_ms;
      g_last_irq_count = ev.count;
      g_state = ES100_STATE_IRQ_PENDING;
    } else if ((uint32_t)(millis() - g_start_ms) >= g_timeout_ms) {
      irq_detach();
      hw_stop();
      g_state = ES100_STATE_TIMEOUT;
      return ES100_RESULT_TIMEOUT;
    }

    return ES100_RESULT_NONE;
  }

  case ES100_STATE_IRQ_PENDING: {
    g_state = ES100_STATE_READING;

    uint8_t irq_status = 0;
    if (!es100_get_irq_status(irq_status)) {
      irq_detach();
      hw_stop();
      g_state = ES100_STATE_FAILED;
      return ES100_RESULT_FAILED;
    }

    g_last_irq_status = irq_status;

    if (irq_status == ES100_IRQ_STATUS_RX_COMPLETE) {
      if (es100_read_time(out_dt)) {
        irq_detach();
        hw_stop();
        g_state = ES100_STATE_DONE;
        return ES100_RESULT_SUCCESS;
      }
    }

    irq_detach();
    hw_stop();
    g_state = ES100_STATE_FAILED;
    return ES100_RESULT_FAILED;
  }

  case ES100_STATE_READING:
  default:
    return ES100_RESULT_NONE;
  }
}

void es100_stop(void) {
  irq_detach();
  hw_stop();
  g_state = ES100_STATE_IDLE;
}

Es100State es100_get_state(void) { return g_state; }

bool es100_is_busy(void) {
  return (g_state == ES100_STATE_WAIT_IRQ ||
          g_state == ES100_STATE_IRQ_PENDING || g_state == ES100_STATE_READING);
}

bool es100_irq_seen(void) { return (g_last_irq_ms != 0UL); }

uint32_t es100_get_last_irq_ms(void) { return g_last_irq_ms; }

uint8_t es100_get_last_irq_count(void) { return g_last_irq_count; }

uint8_t es100_get_last_irq_status(void) { return g_last_irq_status; }

bool es100_get_last_i2c_ok(void) { return g_last_i2c_ok; }

bool es100_get_irq_status(uint8_t &irq_status) {
  uint8_t v = 0;
  if (!i2c_read_regs(ES100_IRQ_STATUS_REG, &v, 1)) {
    return false;
  }

  irq_status = v;
  return true;
}

bool es100_read_time(DateTime &out_dt) {
  uint8_t raw[6];

  if (!i2c_read_regs(ES100_YEAR_REG, raw, sizeof(raw))) {
    return false;
  }

  // Match original code behavior
  const uint16_t year = bcd_to_bin(raw[0]);
  const uint8_t month = bcd_to_bin(raw[1]);
  const uint8_t day = bcd_to_bin(raw[2]);
  const uint8_t hour = bcd_to_bin(raw[3]);
  const uint8_t minute = bcd_to_bin(raw[4]);
  const uint8_t second = bcd_to_bin(raw[5]);

  if (month < 1 || month > 12)
    return false;
  if (day < 1 || day > 31)
    return false;
  if (hour > 23)
    return false;
  if (minute > 59)
    return false;
  if (second > 59)
    return false;

  out_dt = DateTime(year, month, day, hour, minute, second);
  return out_dt.isValid();
}
