#pragma once
#include <stdint.h>
#include <stdbool.h>

void rtc_sqw_begin(int gpio, int edge_mode /*RISING/FALLING*/);
bool rtc_sqw_get_raw(int64_t &edge_esp_us, uint32_t &count);
bool rtc_sqw_is_locked();
