#include "time_sync.h"
#include "config.h"
#include "esp_log.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "pps_isr.h"
#include <RTClib.h>
#include <esp_timer.h>
#include <sys/time.h>

static const char *TAG = "TimeSync";

static TimeSyncStatus s_status{};
static int64_t        s_last_synced_pps_us = 0;
static bool           s_rtc_synced = false;

static constexpr int64_t kJitterAdjustThresholdUs = 500;         // игнорируем мелкие колебания до 500 мкс
static constexpr int64_t kMaxHoldoffUs            = 30000000;  // обязательная коррекция раз в 30 с
static constexpr int64_t kPhaseWindowUs           = 900000;     // окно доверия NMEA↔PPS (±0.9s)

// Для определения “новый PPS или старый”
static uint32_t s_last_pps_count = 0;

// Последняя валидная NMEA секунда и время её получения (в esp_us)
static bool     s_have_nmea = false;
static uint32_t s_last_nmea_utc_sec = 0;
static int64_t  s_last_nmea_esp_us  = 0;

static bool gps_time_to_unix(uint32_t &unix_sec) {
  if (!gps.isReady())
    return false;

  auto &nmea = gps.nmea();
  if (!nmea.isValid())
    return false;

  uint16_t year   = nmea.getYear();
  uint8_t  month  = nmea.getMonth();
  uint8_t  day    = nmea.getDay();
  uint8_t  hour   = nmea.getHour();
  uint8_t  minute = nmea.getMinute();
  uint8_t  second = nmea.getSecond();

  if (year < 2020 || month == 0 || day == 0 || hour > 23 || minute > 59 || second > 60)
    return false;

  DateTime dt(year, month, day, hour, minute, second);
  unix_sec = dt.unixtime();
  return unix_sec > 0;
}

/**
 * Определяем, какая UTC секунда соответствует данному PPS.
 *
 * delta = pps_esp_us - nmea_esp_us
 *  delta >= 0  => NMEA пришло ДО PPS => PPS = nmea_utc_sec + 1
 *  delta <  0  => NMEA пришло ПОСЛЕ PPS => PPS = nmea_utc_sec
 *
 * Порог по модулю delta нужен, чтобы не привязаться к очень старому/задержанному NMEA.
 */
static bool align_pps_utc(int64_t pps_esp_us, uint32_t &pps_utc_sec_out, int64_t &phase_delta_us_out) {
  if (!s_have_nmea) return false;

  int64_t delta = pps_esp_us - s_last_nmea_esp_us;
  phase_delta_us_out = delta;

  if (delta <= -kPhaseWindowUs || delta >= kPhaseWindowUs) {
    return false;
  }

  if (delta >= 0) {
    pps_utc_sec_out = s_last_nmea_utc_sec + 1;
  } else {
    pps_utc_sec_out = s_last_nmea_utc_sec;
  }

  return true;
}

void time_sync_begin() {
  s_status = TimeSyncStatus{};
  s_last_synced_pps_us = 0;
  s_rtc_synced = false;

  s_last_pps_count = 0;
  s_have_nmea = false;
  s_last_nmea_utc_sec = 0;
  s_last_nmea_esp_us = 0;
}

void time_sync_update() {
  // --- 1) Обновляем NMEA (UTC секунду) + фиксируем момент её получения ---
  uint32_t gps_utc = 0;
  if (gps_time_to_unix(gps_utc)) {
    s_status.gps_time_valid = true;

    // Момент обработки валидного NMEA (в esp_us)
    int64_t nmea_arrival_us = esp_timer_get_time();

    // Обновляем, если секунда изменилась или NMEA впервые стало валидным
    if (!s_have_nmea || gps_utc != s_last_nmea_utc_sec) {
      s_have_nmea = true;
      s_last_nmea_utc_sec = gps_utc;
      s_last_nmea_esp_us  = nmea_arrival_us;

      s_status.last_nmea_utc_sec = gps_utc;

      // Для диагностики (не для вычислений): сохраняем “последнюю секунду из GPS”
      pps_set_gps_utc_second(gps_utc);
    }
  } else {
    s_status.gps_time_valid = false;
  }

  // --- 2) Проверяем PPS lock ---
  s_status.pps_locked = pps_is_locked();
  if (!s_status.pps_locked) {
    s_status.phase_aligned = false;
    return;
  }

  // --- 3) Берём raw PPS (время + счётчик) и реагируем только на новый импульс ---
  int64_t  pps_time_us = 0;
  uint32_t pps_count   = 0;
  if (!pps_get_raw(pps_time_us, pps_count)) {
    s_status.phase_aligned = false;
    return;
  }

  if (pps_count == s_last_pps_count) {
    // PPS тот же, не делаем работу повторно
    return;
  }
  s_last_pps_count = pps_count;

  // Уже синхронизировали именно этот PPS (по времени) — доп. защита
  if (pps_time_us == s_last_synced_pps_us)
    return;

  // --- 4) PPS↔NMEA phase alignment: определяем utc_second на этом PPS ---
  uint32_t utc_second = 0;
  int64_t  phase_delta_us = 0;
  if (!align_pps_utc(pps_time_us, utc_second, phase_delta_us) || utc_second == 0) {
    s_status.phase_aligned = false;
    s_status.last_phase_delta_us = phase_delta_us;

    ESP_LOGW(TAG, "PPS received but cannot align with NMEA (delta=%lld us, have_nmea=%d)",
             (long long)phase_delta_us, (int)s_have_nmea);
    return;
  }

  s_status.phase_aligned = true;
  s_status.last_phase_delta_us = phase_delta_us;

  // --- 5) Рассчитываем target wall-clock по PPS и текущему времени ---
  int64_t now_us = esp_timer_get_time();
  int64_t age_us = now_us - pps_time_us;
  if (age_us < 0) age_us = 0;

  // Целевое системное время (UTC) на текущий момент:
  // PPS соответствует utc_second*1e6, плюс прошедшее время age_us
  int64_t target_us = (int64_t)utc_second * 1000000LL + age_us;

  timeval current_tv{};
  gettimeofday(&current_tv, nullptr);
  int64_t current_us = (int64_t)current_tv.tv_sec * 1000000LL + (int64_t)current_tv.tv_usec;

  int64_t delta_us = target_us - current_us;

  bool need_adjust = !s_status.synced ||
                     (delta_us < -kJitterAdjustThresholdUs || delta_us > kJitterAdjustThresholdUs) ||
                     ((now_us - s_status.last_sync_us) > kMaxHoldoffUs);

  if (need_adjust) {
    int64_t sec64  = target_us / 1000000LL;
    int64_t usec64 = target_us % 1000000LL;
    if (usec64 < 0) { usec64 += 1000000LL; sec64 -= 1; }

    timeval tv{};
    tv.tv_sec  = (time_t)sec64;
    tv.tv_usec = (suseconds_t)usec64;

    settimeofday(&tv, nullptr);

    s_status.last_sync_us     = now_us;
    s_status.last_utc_second  = (uint32_t)tv.tv_sec;
    s_last_synced_pps_us      = pps_time_us;

    ESP_LOGI(TAG,
             "System time synced: %ld.%06ld (delta %lld us, age %lld us, pps_utc %lu, phase %lld us, pps %lld, cnt %lu)",
             (long)tv.tv_sec, (long)tv.tv_usec,
             (long long)delta_us, (long long)age_us,
             (unsigned long)utc_second,
             (long long)phase_delta_us,
             (long long)pps_time_us,
             (unsigned long)pps_count);

    // Обновляем RTC один раз после первой успешной синхронизации
    if (!s_rtc_synced && rtc.isReady()) {
      rtc.setTime((uint32_t)tv.tv_sec);
      s_rtc_synced = true;
    }
  }

  s_status.synced                = true;
  s_status.last_pps_timestamp_us = pps_time_us;
  s_status.last_offset_us        = delta_us;
}

TimeSyncStatus time_sync_status() { return s_status; }
