#pragma once

#include <stdint.h>

struct TimeSyncStatus {
  bool     synced = false;
  bool     gps_time_valid = false;
  bool     pps_locked = false;
  uint32_t last_utc_second = 0;
  int64_t  last_pps_timestamp_us = 0;
  int64_t  last_sync_us = 0;
  int64_t  last_offset_us = 0;
};

/**
 * Инициализация синхронизации системного времени
 */
void time_sync_begin();

/**
 * Обновление состояния синхронизации (вызывать из loop)
 */
void time_sync_update();

/**
 * Текущее состояние синхронизации
 */
TimeSyncStatus time_sync_status();
