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

// Порог: игнорируем мелкий дрейф системных часов (до 1 мс)
static constexpr int64_t kJitterAdjustThresholdUs = 1000;

// Обязательная коррекция системного времени раз в N секунд (если включён settimeofday)
static constexpr int64_t kMaxHoldoffUs = 30000000;

// Phase alignment: окно доверия (±0.9s)
static constexpr int64_t kPhaseWindowUs = 900000;

// RTC fallback: как часто подправлять якорь по RTC (чтобы не уплывать)
static constexpr int64_t kRtcResyncPeriodUs = 10000000; // 10 секунд
static int64_t s_last_rtc_resync_us = 0;

// Для определения “новый PPS или старый”
static uint32_t s_last_pps_count = 0;

// Последняя валидная NMEA секунда и момент её получения (esp_us)
static bool     s_have_nmea = false;
static uint32_t s_last_nmea_utc_sec = 0;
static int64_t  s_last_nmea_esp_us  = 0;

static uint32_t s_last_sqw_count = 0;
static constexpr int64_t kSqwAgeWindowUs = 900000; // если обработали позже — лучше не переякориваться
static bool s_in_rtc_fallback = false;

static bool s_logged_no_sqw = false;
static bool s_prev_sqw_locked = false;
static bool s_prev_have_sqw_edge = false; // чтобы логировать "signal acquired" или "signal lost"
static bool s_logged_sqw_warmup = false;

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

static void set_anchor(TimeSource src, int64_t anchor_utc_us, int64_t anchor_esp_us) {
  s_status.source = src;
  s_status.anchor_utc_us = anchor_utc_us;
  s_status.anchor_esp_us = anchor_esp_us;
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
  if (!s_have_nmea)
    return false;

  int64_t delta = pps_esp_us - s_last_nmea_esp_us;
  phase_delta_us_out = delta;

  if (delta <= -kPhaseWindowUs || delta >= kPhaseWindowUs)
    return false;

  if (delta >= 0) {
    pps_utc_sec_out = s_last_nmea_utc_sec + 1;
  } else {
    pps_utc_sec_out = s_last_nmea_utc_sec;
  }
  return true;
}

bool time_sync_esp_to_utc_us(int64_t esp_us, int64_t &utc_us_out) {
  if (s_status.source == TimeSource::NONE || s_status.anchor_esp_us == 0 || s_status.anchor_utc_us == 0)
    return false;

  utc_us_out = s_status.anchor_utc_us + (esp_us - s_status.anchor_esp_us);
  return true;
}

static bool set_system_time_from_rtc_on_second_edge(uint32_t timeout_ms = 1500) {
  if (!rtc.isReady()) return false;

  uint32_t sec0 = rtc.unixTime();
  int64_t  t0_us = esp_timer_get_time();

  // ждём, пока RTC секунда сменится
  while ((esp_timer_get_time() - t0_us) < (int64_t)timeout_ms * 1000LL) {
    uint32_t sec1 = rtc.unixTime();
    if (sec1 != sec0) {
      // секунда сменилась — выставляем ровно на границу новой секунды
      timeval tv{};
      tv.tv_sec  = (time_t)sec1;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);
      return true;
    }
    delay(5); // не грузим I2C/CPU
  }

  // таймаут: ставим как есть (лучше, чем 1970), но без выравнивания
  timeval tv{};
  tv.tv_sec  = (time_t)sec0;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  return false;
}

void time_sync_begin() {
  s_status = TimeSyncStatus{};
  s_last_synced_pps_us = 0;
  s_rtc_synced = false;

  s_last_pps_count = 0;
  s_have_nmea = false;
  s_last_nmea_utc_sec = 0;
  s_last_nmea_esp_us = 0;

  s_last_rtc_resync_us = 0;

  s_last_sqw_count = 0;
  rtc_sqw_begin(RTC_SQW_PIN, RISING);

  s_in_rtc_fallback = false;
  s_logged_no_sqw = false;
  s_prev_sqw_locked = false;
  s_prev_have_sqw_edge = false;
  s_logged_sqw_warmup = false;

  // Инициализация системного времени по RTC
  if (rtc.isReady()) {
    bool aligned = set_system_time_from_rtc_on_second_edge(1500);
    ESP_LOGI(TAG, "Boot time from RTC: %s", aligned ? "aligned to second" : "not aligned (timeout)");
  }
}

void time_sync_update() {
  // --- 1) NMEA: обновляем секунду и фиксируем момент её получения ---
  uint32_t gps_utc = 0;
  if (gps_time_to_unix(gps_utc)) {
    s_status.gps_time_valid = true;

    int64_t nmea_arrival_us = esp_timer_get_time();

    // Обновляем статус для диагностики всегда
    s_status.last_nmea_utc_sec = gps_utc;

    // Запоминаем “последнюю секунду” и момент, когда мы её получили (для phase alignment)
    if (!s_have_nmea || gps_utc != s_last_nmea_utc_sec) {
      s_have_nmea = true;
      s_last_nmea_utc_sec = gps_utc;
      s_last_nmea_esp_us  = nmea_arrival_us;

      // Для диагностики (не для вычисления PPS секунды)
      pps_set_gps_utc_second(gps_utc);
    }
  } else {
    s_status.gps_time_valid = false;
  }

  // --- 2) PPS lock? ---
  s_status.pps_locked = pps_is_locked();

  // --- 3) Если PPS нет — fallback на RTC ---
  // if (!s_status.pps_locked) {
  //   s_status.phase_aligned = false;

  //   if (!rtc.isReady()) {
  //     // RTC не готов — источника времени нет
  //     s_status.source = TimeSource::NONE;
  //     return;
  //   }

  //   int64_t now_us = esp_timer_get_time();

  //   // Первый вход в RTC режим: якорим по RTC
  //   if (s_status.source != TimeSource::RTC) {
  //     uint32_t rtc_sec = rtc.unixTime();
  //     set_anchor(TimeSource::RTC, (int64_t)rtc_sec * 1000000LL, now_us);
  //     s_last_rtc_resync_us = now_us;

  //     ESP_LOGW(TAG, "GPS/PPS lost -> RTC fallback. rtc=%lu", (unsigned long)rtc_sec);

  //     // (опционально) можно один раз выставить системное время по RTC
  //     timeval tv{ .tv_sec = (time_t)rtc_sec, .tv_usec = 0 };
  //     settimeofday(&tv, nullptr);

  //     s_status.synced = true;
  //     s_status.last_sync_us = now_us;
  //     s_status.last_offset_us = 0;
  //     return;
  //   }

  //   // Уже в RTC режиме: периодически подтягиваем якорь, чтобы не уплывать
  //   if ((now_us - s_last_rtc_resync_us) > kRtcResyncPeriodUs) {
  //     uint32_t rtc_sec = rtc.unixTime();
  //     int64_t rtc_utc_us = (int64_t)rtc_sec * 1000000LL;

  //     int64_t est_utc_us = 0;
  //     if (time_sync_esp_to_utc_us(now_us, est_utc_us)) {
  //       int64_t err_us = rtc_utc_us - est_utc_us;

  //       // Сдвигаем UTC-якорь так, чтобы "сейчас" совпало с RTC
  //       s_status.anchor_utc_us += err_us;

  //       s_last_rtc_resync_us = now_us;

  //       ESP_LOGI(TAG, "RTC resync: err=%lld us (anchor adjusted)", (long long)err_us);

  //       // (опционально) поддерживаем системное время близким к RTC
  //       timeval current_tv{};
  //       gettimeofday(&current_tv, nullptr);
  //       int64_t current_us = (int64_t)current_tv.tv_sec * 1000000LL + (int64_t)current_tv.tv_usec;
  //       int64_t delta_us = rtc_utc_us - current_us;

  //       if (delta_us < -kJitterAdjustThresholdUs || delta_us > kJitterAdjustThresholdUs) {
  //         timeval tv{};
  //         tv.tv_sec  = (time_t)(rtc_utc_us / 1000000LL);
  //         tv.tv_usec = (suseconds_t)(rtc_utc_us % 1000000LL);
  //         settimeofday(&tv, nullptr);
  //       }
  //     } else {
  //       // На всякий случай переякорим
  //       set_anchor(TimeSource::RTC, rtc_utc_us, now_us);
  //       s_last_rtc_resync_us = now_us;
  //     }
  //   }

  //   s_status.synced = true;
  //   return;
  // }

  // --- 3) Если PPS нет — fallback на RTC (через SQW 1Hz) ---
  if (!s_status.pps_locked) {
    s_status.phase_aligned = false;

    if (!rtc.isReady()) {
      s_status.source = TimeSource::NONE;
      s_status.synced = false;
      return;
    }

    int64_t sqw_edge_us = 0;
    uint32_t sqw_count  = 0;

    bool have_edge = rtc_sqw_get_raw(sqw_edge_us, sqw_count);
    bool locked    = rtc_sqw_is_locked();




    // --- LOG: SQW signal/lock transitions (no spam) ---
    if (have_edge != s_prev_have_sqw_edge) {
      s_prev_have_sqw_edge = have_edge;
      if (have_edge) ESP_LOGI(TAG, "RTC SQW signal acquired");
      else           ESP_LOGW(TAG, "RTC SQW signal lost");
    }

    if (locked != s_prev_sqw_locked) {
      s_prev_sqw_locked = locked;
      if (locked) ESP_LOGI(TAG, "RTC SQW lock acquired");
      else        ESP_LOGW(TAG, "RTC SQW lock lost");
    }

    // --- дружелюбная логика ---
    // a) SQW сигнала нет -> NOSYNC
    if (!have_edge) {
      s_status.source = TimeSource::NONE;
      s_status.synced = false;

      if (!s_logged_no_sqw) {
        s_logged_no_sqw = true;
        ESP_LOGW(TAG, "RTC ready but SQW no signal -> NOSYNC");
      }
      s_logged_sqw_warmup = false;
      return;
    }

    // b) SQW сигнал есть, но lock ещё не набран -> RTC_DEGRADED (warmup)
    if (!locked) {
      s_logged_no_sqw = false;

      if (!s_logged_sqw_warmup) {
        s_logged_sqw_warmup = true;
        ESP_LOGI(TAG, "RTC SQW warmup: signal present, waiting lock...");
      }

      s_status.source = TimeSource::RTC;
      s_status.synced = true;
      return;
    }

    // c) SQW locked -> RTC OK
    s_logged_no_sqw = false;
    s_logged_sqw_warmup = false;

    // Переход в RTC fallback (один раз)
    if (!s_in_rtc_fallback) {
      s_in_rtc_fallback = true;

      // Для красивого лога вытащим секунду RTC
      uint32_t rtc_sec = rtc.unixTime();
      ESP_LOGW(TAG, "GPS/PPS lost -> RTC+SQW fallback. rtc=%lu", (unsigned long)rtc_sec);
    }

    // реагируем только на новый фронт SQW
    if (sqw_count != s_last_sqw_count) {
      s_last_sqw_count = sqw_count;

      int64_t now_us = esp_timer_get_time();
      int64_t age_us = now_us - sqw_edge_us;
      if (age_us < 0) age_us = 0;

      // если слишком поздно обработали тик — можно промахнуться на секунду, не трогаем якорь
      if (age_us <= kSqwAgeWindowUs) {
        uint32_t rtc_sec = rtc.unixTime();
        int64_t rtc_utc_us = (int64_t)rtc_sec * 1000000LL;

        // ---- анти-±1 сек защита ----
        // Сравниваем RTC секунду с нашей оценкой (если якорь уже был)
        int64_t est_utc_us = 0;
        if (time_sync_esp_to_utc_us(sqw_edge_us, est_utc_us)) {
          int64_t diff = rtc_utc_us - est_utc_us;
          // если очень похоже на +1 сек или -1 сек — поправим
          if (diff > 500000 && diff < 1500000)      rtc_utc_us -= 1000000LL;
          else if (diff < -500000 && diff > -1500000) rtc_utc_us += 1000000LL;
        }

        set_anchor(TimeSource::RTC, rtc_utc_us, sqw_edge_us);
        s_last_rtc_resync_us = now_us;

        // (опционально) дисциплина системного времени
        int64_t target_us = rtc_utc_us + age_us;

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

          s_status.last_sync_us    = now_us;
          s_status.last_offset_us  = delta_us;
          s_status.last_utc_second = (uint32_t)tv.tv_sec;
        }
      }
    }

    s_status.source = TimeSource::RTC;
    s_status.synced = true;
    return;
  }

  // --- 4) PPS есть: берём raw PPS ---
  int64_t  pps_time_us = 0;
  uint32_t pps_count   = 0;
  if (!pps_get_raw(pps_time_us, pps_count)) {
    s_status.phase_aligned = false;
    return;
  }

  // Реагируем только на новый импульс
  if (pps_count == s_last_pps_count)
    return;
  s_last_pps_count = pps_count;

  // Уже синхронизировали этот PPS (доп. защита)
  if (pps_time_us == s_last_synced_pps_us)
    return;

  // --- 5) PPS↔NMEA phase alignment ---
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

  // GPS режим: якорь = точный PPS
  set_anchor(TimeSource::GPS_PPS, (int64_t)utc_second * 1000000LL, pps_time_us);

  // Возврат из RTC fallback (один раз)
  if (s_in_rtc_fallback) {
    s_in_rtc_fallback = false;
    ESP_LOGI(TAG, "GPS/PPS restored -> GPS_PPS mode");
  }

  // --- 6) (Опционально) дисциплинируем системные часы через settimeofday ---
  int64_t now_us = esp_timer_get_time();
  int64_t age_us = now_us - pps_time_us;
  if (age_us < 0) age_us = 0;

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

    // RTC обновляем один раз после первой успешной GPS синхронизации
    if (!s_rtc_synced && rtc.isReady()) {
      rtc.setTime((uint32_t)tv.tv_sec);
      s_rtc_synced = true;
    }
  }

  s_status.synced                = true;
  s_status.last_pps_timestamp_us = pps_time_us;
  s_status.last_offset_us        = delta_us;
  s_status.last_utc_second       = utc_second;
}

TimeSyncStatus time_sync_status() { return s_status; }

TimeSyncState time_sync_state() {
  if (!s_status.synced)
    return TimeSyncState::NONE;

  if (s_status.source == TimeSource::GPS_PPS) {
    if (s_status.pps_locked && s_status.phase_aligned)
      return TimeSyncState::GPS_OK;
    return TimeSyncState::GPS_DEGRADED;
  }

  if (s_status.source == TimeSource::RTC) {
    if (rtc_sqw_is_locked())
      return TimeSyncState::RTC_OK;
    return TimeSyncState::RTC_DEGRADED;
  }

  return TimeSyncState::NONE;
}

int64_t time_sync_estimate_accuracy_us() {
  if (!s_status.synced)
    return -1;

  switch (s_status.source) {
    case TimeSource::GPS_PPS: {
      int64_t acc = llabs(s_status.last_phase_delta_us);

      // запас на таймер и ISR
      acc += 10;

      // если PPS был давно — чуть ухудшаем
      int64_t now_us = esp_timer_get_time();
      int64_t age_us = now_us - s_status.last_pps_timestamp_us;
      if (age_us > 1000000)
        acc += age_us / 1000000; // ~1µs на секунду

      return acc;
    }

    case TimeSource::RTC: {
      if (!rtc_sqw_is_locked()) return -1;
    
      // основная ошибка: ISR latency + чтение rtc.unixTime() (I2C) + анти-±1с логика
      // Консервативно: 500–2000 us (зависит от нагрузки)
      int64_t acc = 1500;
    
      // лёгкое ухудшение, если давно не было нового SQW тика
      int64_t now_us = esp_timer_get_time();
      int64_t age_us = now_us - s_status.anchor_esp_us;
      if (age_us > 1000000) acc += age_us / 1000; // +1ms за каждую секунду просрочки
    
      return acc;
    }

    default:
      return -1;
  }
}
