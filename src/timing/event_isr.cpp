#include "event_isr.h"
#include <Arduino.h>
#include <esp_timer.h>

#define EVENT_DEADTIME_US 100000 // 100 ms

static volatile bool     s_event_captured = false;
static volatile int64_t  s_event_time_us  = 0;
static volatile int64_t  s_last_event_us  = 0;

static void IRAM_ATTR event_isr_handler() {
  int64_t now = esp_timer_get_time();

  // Если первое событие ещё не обработано — игнорируем всё
  if (s_event_captured) {
    return;
  }

  // Dead-time
  if ((now - s_last_event_us) < EVENT_DEADTIME_US) {
    return;
  }

  s_last_event_us  = now;
  s_event_time_us  = now;
  s_event_captured = true;
}

void event_isr_init(int gpio_pin) {
  pinMode(gpio_pin, INPUT_PULLUP);
  attachInterrupt(gpio_pin, event_isr_handler, FALLING);
}

bool event_isr_get(int64_t &timestamp_us) {
  if (!s_event_captured)
    return false;

  noInterrupts();
  timestamp_us = s_event_time_us;
  s_event_captured = false;
  interrupts();

  return true;
}
