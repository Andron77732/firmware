#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "rtc_sqw_isr.h"

enum class TimeSource : uint8_t {
  NONE   = 0,
  GPS_PPS= 1,
  RTC    = 2,
};

enum class TimeSyncState : uint8_t {
  NONE = 0,        // времени нет

  GPS_OK = 1,          // PPS + phase aligned
  GPS_DEGRADED = 2,    // PPS есть, но phase lost / holdover

  RTC_OK = 3,          // RTC + SQW стабилен
  RTC_DEGRADED = 4     // RTC есть, но SQW нестабилен / неточен
};

struct TimeSyncStatus {
  bool     synced = false;
  bool     gps_time_valid = false;
  bool     pps_locked = false;

  // Состояние phase alignment (PPS↔NMEA)
  bool     phase_aligned = false;
  int64_t  last_phase_delta_us = 0; // pps_us - nmea_arrival_us
  uint32_t last_nmea_utc_sec = 0;

  // Последняя UTC секунда, соответствующая ПОСЛЕДНЕМУ PPS (в GPS режиме)
  uint32_t last_utc_second = 0;

  // Последний PPS timestamp (esp us) и когда мы синхронизировали системное время (esp us)
  int64_t  last_pps_timestamp_us = 0;
  int64_t  last_sync_us = 0;

  // Ошибка (target - current) в микросекундах в момент последнего расчёта
  int64_t  last_offset_us = 0;

  // Источник времени и якорь для esp_us -> utc_us
  TimeSource source = TimeSource::NONE;
  int64_t  anchor_utc_us = 0;  // UTC (us) в момент якоря
  int64_t  anchor_esp_us = 0;  // esp_timer (us) в момент якоря
};

/**
 * Инициализация подсистемы синхронизации
 */
void time_sync_begin();

/**
 * Обновление синхронизации (вызывать в loop)
 */
void time_sync_update();

/**
 * Получить статус
 */
TimeSyncStatus time_sync_status();

/**
 * Получить состояние синхронизации
 */
TimeSyncState time_sync_state();

/**
 * Перевод esp_timer timestamp (us) -> UTC (us) по текущему якорю.
 * Работает в GPS_PPS режиме (макс. точность) и в RTC режиме (fallback).
 *
 * @return false если пока нет источника времени (нет PPS и RTC не готов).
 */
bool time_sync_esp_to_utc_us(int64_t esp_us, int64_t &utc_us_out);

/**
 * Оценка точности синхронизации в микросекундах
 * @return точность в микросекундах (±) или -1 если нет синхронизации
 */
int64_t time_sync_estimate_accuracy_us();