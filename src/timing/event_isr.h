#pragma once
#include <stdint.h>
#include <stdbool.h>

void event_isr_init(int gpio_pin);

bool event_isr_get(int64_t &timestamp_us);