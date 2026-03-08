#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * Инициализация PPS GPIO
 */
void pps_init(int gpio_pin);

/**
 * PPS lock с гистерезисом:
 * - lock после нескольких подряд валидных PPS,
 * - loss после увеличенного timeout (переживает одиночный пропуск PPS).
 */
bool pps_is_locked();

/**
 * Raw PPS: esp_timer timestamp + счётчик PPS
 * Возвращает false если PPS ещё не приходил.
 */
bool pps_get_raw(int64_t &pps_time_us, uint32_t &pps_count);

/**
 * (Опционально) сохранить последнюю секунду из GPS (unix seconds) для диагностики
 */
void pps_set_gps_utc_second(uint32_t gps_utc_sec);

/**
 * (Опционально) прочитать последнюю секунду из GPS (unix seconds)
 */
uint32_t pps_get_last_gps_utc_second();
