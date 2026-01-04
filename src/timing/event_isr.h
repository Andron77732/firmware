#pragma once
#include <stdint.h>
#include <stdbool.h>

void event_isr_init(int gpio_pin);

// Проверка наличия события
bool event_isr_has_event();

// Получить timestamp (us)
int64_t event_isr_get_timestamp_us();