#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * Инициализация PPS GPIO
 */
void pps_init(int gpio_pin);

/**
 * Есть ли валидный PPS
 */
bool pps_is_locked();

/**
 * Получить последний PPS timestamp
 * @return true — данные получены
 */
bool pps_get(int64_t &pps_time_us, uint32_t &utc_second);

/**
 * Установить текущую UTC секунду из GPS
 */
void pps_set_utc_second(uint32_t utc_sec);
