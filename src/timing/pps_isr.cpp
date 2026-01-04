#include "pps_isr.h"
#include <Arduino.h>
#include <esp_timer.h>
#include "esp_log.h"

static const char* TAG = "PPS";

// Внутреннее состояние PPS
static volatile int64_t  s_pps_time_us = 0;
static volatile uint32_t s_utc_second  = 0;
static volatile bool     s_pps_valid   = false;
static volatile int64_t  s_last_pps_us = 0;
static volatile uint32_t s_pps_count   = 0;  // Счетчик PPS сигналов

// Обработчик прерывания PPS (вызывается на rising edge GPIO)
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

void pps_set_utc_second(uint32_t utc_sec) {
  // Вызывается из GPS/NMEA кода при получении времени
  s_utc_second = utc_sec;
  ESP_LOGD(TAG, "UTC second set to %lu", utc_sec);
}

bool pps_is_locked() {
  int64_t now = esp_timer_get_time();
  // Проверяем, что последний PPS был менее 1.5 секунд назад
  bool locked = s_pps_valid && ((now - s_last_pps_us) < 1500000);
  
  // Логируем изменения состояния lock (только при переходе)
  static bool last_locked_state = false;
  if (locked != last_locked_state) {
    if (locked) {
      ESP_LOGI(TAG, "PPS lock acquired");
    } else if (s_pps_valid) {
      int64_t age_ms = (now - s_last_pps_us) / 1000;
      ESP_LOGW(TAG, "PPS lock lost (last PPS %lld ms ago)", age_ms);
    }
    last_locked_state = locked;
  }
  
  return locked;
}

bool pps_get(int64_t &pps_time_us, uint32_t &utc_second) {
  if (!s_pps_valid)
    return false;

  noInterrupts();
  pps_time_us = s_pps_time_us;
  utc_second  = s_utc_second;
  uint32_t count = s_pps_count;
  interrupts();

  ESP_LOGD(TAG, "PPS get: time=%lld us, UTC=%lu, count=%lu", pps_time_us, utc_second, count);
  return true;
}
