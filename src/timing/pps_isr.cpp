#include "pps_isr.h"
#include <Arduino.h>
#include <esp_timer.h>
#include "esp_log.h"

static const char* TAG = "PPS";

// PPS timing
static volatile int64_t  s_pps_time_us = 0;
static volatile int64_t  s_last_pps_us = 0;
static volatile bool     s_pps_valid   = false;
static volatile uint32_t s_pps_count   = 0;

// Только для диагностики
static volatile uint32_t s_last_gps_utc_sec = 0;

static constexpr int64_t kLossTimeoutUs = 1500000; // 1.5s

static void IRAM_ATTR pps_isr_handler() {
  int64_t now = esp_timer_get_time();
  s_pps_time_us = now;
  s_last_pps_us = now;
  s_pps_valid   = true;
  s_pps_count++;
}

void pps_init(int gpio_pin) {
  pinMode(gpio_pin, INPUT);
  attachInterrupt(gpio_pin, pps_isr_handler, RISING);
  ESP_LOGI(TAG, "Initialized on GPIO %d (RISING edge)", gpio_pin);
}

void pps_set_gps_utc_second(uint32_t gps_utc_sec) {
  s_last_gps_utc_sec = gps_utc_sec;
  ESP_LOGV(TAG, "GPS UTC second set to %lu", (unsigned long)gps_utc_sec);
}

uint32_t pps_get_last_gps_utc_second() {
  return s_last_gps_utc_sec;
}

bool pps_is_locked() {
  int64_t now = esp_timer_get_time();
  bool locked = s_pps_valid && ((now - s_last_pps_us) < kLossTimeoutUs);

  static bool last_locked_state = false;
  if (locked != last_locked_state) {
    if (locked) {
      ESP_LOGI(TAG, "PPS lock acquired");
    } else if (s_pps_valid) {
      int64_t age_ms = (now - s_last_pps_us) / 1000;
      ESP_LOGW(TAG, "PPS lock lost (last PPS %lld ms ago)", (long long)age_ms);
    }
    last_locked_state = locked;
  }
  return locked;
}

bool pps_get_raw(int64_t &pps_time_us, uint32_t &pps_count) {
  if (!s_pps_valid) return false;

  noInterrupts();
  pps_time_us = s_pps_time_us;
  pps_count   = s_pps_count;
  interrupts();

  return true;
}
