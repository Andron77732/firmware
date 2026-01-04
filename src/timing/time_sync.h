#pragma once

#include <stdint.h>

struct TimeSyncStatus {
  bool     synced = false;
  bool     gps_time_valid = false;
  bool     pps_locked = false;

  // Последнее выставленное системное время (секунда)
  uint32_t last_utc_second = 0;

  // Последний PPS timestamp (esp us) и когда мы синхронизировали (esp us)
  int64_t  last_pps_timestamp_us = 0;
  int64_t  last_sync_us = 0;

  // Ошибка (target - current) в микросекундах на момент последнего расчёта
  int64_t  last_offset_us = 0;

  // Phase alignment диагностика
  bool     phase_aligned = false;
  int64_t  last_phase_delta_us = 0;  // pps_us - nmea_arrival_us
  uint32_t last_nmea_utc_sec = 0;
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
