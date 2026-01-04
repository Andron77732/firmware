#include "event_isr.h"
#include <Arduino.h>
#include <esp_timer.h>

#define EVENT_DEADTIME_US 100000 // 100ms

static volatile bool     s_event_pending = false;
static volatile int64_t  s_event_time_us = 0;
static volatile int64_t  s_last_event_us = 0;

static void IRAM_ATTR event_isr_handler() {
  int64_t now = esp_timer_get_time();

  if ((now - s_last_event_us) < EVENT_DEADTIME_US) {
    return;
  }

  s_last_event_us = now;
  s_event_time_us = now;
  s_event_pending = true;
}

void event_isr_init(int gpio_pin) {
  pinMode(gpio_pin, INPUT_PULLUP);
  attachInterrupt(gpio_pin, event_isr_handler, FALLING);
}

bool event_isr_has_event() {
  return s_event_pending;
}

int64_t event_isr_get_timestamp_us() {
  noInterrupts();
  s_event_pending = false;
  int64_t t = s_event_time_us;
  interrupts();
  return t;
}
