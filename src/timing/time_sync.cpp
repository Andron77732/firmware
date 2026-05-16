#include "time_sync.h"
#include "config.h"
#include "esp_log.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "storage/settings.h"
#include "pps_isr.h"
#include <RTClib.h>
#include <esp_timer.h>
#include <sys/time.h>

static const char *TAG = "TimeSync";

static TimeSyncStatus s_status{};
static int64_t        s_last_synced_pps_us = 0;
static bool           s_rtc_synced = false;
static TimeSyncStateCallback s_state_callback = nullptr;
static TimeSyncState s_last_state = TimeSyncState::NONE;

// Порог: игнорируем мелкий дрейф системных часов (до 1 мс)
static constexpr int64_t kJitterAdjustThresholdUs = 1000;

// Обязательная коррекция системного времени раз в N секунд (если включён settimeofday)
static constexpr int64_t kMaxHoldoffUs = 30000000;

// Phase alignment: окно доверия (±0.99s)
static constexpr int64_t kPhaseWindowUs = 990000;

// RTC fallback: как часто подправлять якорь по RTC (чтобы не уплывать)
static constexpr int64_t kRtcResyncPeriodUs = 10000000; // 10 секунд
static int64_t s_last_rtc_resync_us = 0;
static constexpr uint8_t kRtcFallbackWarmupTicks = 3;
static constexpr int64_t kRtcFallbackLargeStepGuardUs = 100000;     // 100 ms
static constexpr int64_t kRtcFallbackInitialAnchorMaxAgeUs = 50000; // 50 ms
static constexpr int64_t kNmeaFreshnessUs = 1500000;                // 1.5 s
static constexpr int64_t kGpsCandidateJumpGuardUs = 5000000;        // 5 s
static constexpr uint8_t kGpsRelockWarmupPps = 2;
static constexpr int64_t kGpsRelockLargeStepGuardUs = 100000;       // 100 ms

// Анти-±1с защита: якорь считаем "свежим" только в пределах окна
static constexpr int64_t kAnchorFreshnessUs = 3000000; // 3 секунды

// Дисциплина RTC по PPS: период и окно выравнивания
static constexpr int64_t kRtcPpsSyncPeriodUs   = 10LL * 60LL * 1000000LL; // 10 минут
static constexpr int64_t kRtcPpsAlignWindowUs  = 3000; // 3 ms
static int64_t s_last_rtc_pps_sync_us = 0;
static bool    s_rtc_pps_pending = false;
static uint32_t s_rtc_pps_target_sec = 0;
static uint32_t s_rtc_pps_target_count = 0;

// Оценка точности: базовые допуски на джиттер/латентность
static constexpr int64_t kPpsIsrJitterUs     = 20;    // ISR + чтение таймера
static constexpr int64_t kRtcBaseJitterUs    = 1500;  // I2C + ISR + анти-±1с логика
static constexpr int64_t kGpsPhaseResidualUs = 200;   // остаточная неопределенность PPS<->UTC после phase lock
static constexpr int64_t kRtcAgingCapUs      = 10000; // максимум +10ms штрафа за "просрочку" тика

// Для определения “новый PPS или старый”
static uint32_t s_last_pps_count = 0;

// Последняя валидная NMEA секунда и момент её получения (esp_us)
static bool     s_have_nmea = false;
static uint32_t s_last_nmea_utc_sec = 0;
static int64_t  s_last_nmea_esp_us  = 0;

static uint32_t s_last_sqw_count = 0;
static uint32_t s_rtc_anchor_sqw_count = 0; // sqw_count в момент set_anchor(RTC,...)
static int64_t  s_last_sqw_edge_us = 0; // последний обработанный фронт SQW (для accuracy)
static constexpr int64_t kSqwAgeWindowUs = 900000; // если обработали позже — лучше не переякориваться
static bool s_in_rtc_fallback = false;
static bool s_have_rtc_anchor = false;

static bool s_logged_no_sqw = false;
static bool s_prev_sqw_locked = false;
static bool s_prev_have_sqw_edge = false; // чтобы логировать "signal acquired" или "signal lost"
static bool s_logged_sqw_warmup = false;
static bool s_logged_rtc_only = false;
static bool s_log_rtc_fallback_delta = false;
static bool s_rtc_fallback_guard_active = false;
static bool s_rtc_fallback_guard_logged = false;
static uint8_t s_rtc_fallback_ticks = 0;
static bool s_logged_rtc_anchor_wait = false;
static bool s_gps_relock_guard_active = false;
static bool s_gps_relock_guard_logged = false;
static uint8_t s_gps_relock_pps_ok = 0;

static bool is_auto_sync_enabled() {
  return settings.getSync().auto_sync;
}

static void notify_state_change_if_needed() {
  TimeSyncState state = time_sync_state();
  if (state == s_last_state)
    return;
  s_last_state = state;
  if (s_state_callback)
    s_state_callback(state);
}

// Фильтр phase_delta: медиана по 5 последним значениям (устойчиво к выбросам NMEA)
static int64_t s_phase_deltas[5] = {0};
static uint8_t s_phase_delta_idx = 0;
static uint8_t s_phase_delta_count = 0;

static void reset_phase_delta_filter() {
  for (int i = 0; i < 5; ++i) s_phase_deltas[i] = 0;
  s_phase_delta_idx = 0;
  s_phase_delta_count = 0;
}

static int64_t median5(const int64_t v[5]) {
  int64_t a[5] = {v[0], v[1], v[2], v[3], v[4]};
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 5; ++j) {
      if (a[j] < a[i]) {
        int64_t t = a[i];
        a[i] = a[j];
        a[j] = t;
      }
    }
  }
  return a[2];
}

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
  if (src == TimeSource::RTC) {
    s_have_rtc_anchor = true;
  } else {
    s_have_rtc_anchor = false;
    s_rtc_anchor_sqw_count = 0;
  }
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

  int64_t now_us = esp_timer_get_time();
  int64_t nmea_age_us = now_us - s_last_nmea_esp_us;
  if (nmea_age_us < 0 || nmea_age_us > kNmeaFreshnessUs)
    return false;

  // Фильтр phase_delta
  int64_t delta_raw = pps_esp_us - s_last_nmea_esp_us;
  s_phase_deltas[s_phase_delta_idx] = delta_raw;
  s_phase_delta_idx = (s_phase_delta_idx + 1) % 5;
  if (s_phase_delta_count < 5) s_phase_delta_count++;

  int64_t delta = delta_raw;
  if (s_phase_delta_count == 5) {
    delta = median5(s_phase_deltas);
  }

  phase_delta_us_out = delta;

  // --- "мёртвая зона" около ±1 секунды (защита от ±1s ошибки) ---
  static constexpr int64_t kPhaseDeadbandUs = 5000; // 5 ms, можно 3000..10000

  int64_t abs_delta = llabs(delta);

  // 1) если слишком близко к ровно 1 секунде — не доверяем
  if (abs_delta >= (1000000LL - kPhaseDeadbandUs)) {
    return false;
  }

  // 2) обычное окно доверия (kPhaseWindowUs)
  if (abs_delta > kPhaseWindowUs) {
    return false;
  }

  if (delta >= 0) {
    pps_utc_sec_out = s_last_nmea_utc_sec + 1;
  } else {
    pps_utc_sec_out = s_last_nmea_utc_sec;
  }

  // Если время уже синхронизировано, не принимаем резкие скачки
  // кандидата UTC от PPS, чтобы отфильтровать поздние/битые NMEA.
  // При auto_sync=false системные часы могут не обновляться, поэтому
  // сравниваем с UTC-оценкой по текущему якорю, а не только с gettimeofday().
  if (s_status.synced) {
    int64_t current_us = 0;
    if (!time_sync_esp_to_utc_us(pps_esp_us, current_us)) {
      timeval tv{};
      gettimeofday(&tv, nullptr);
      current_us = (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
    }
    int64_t candidate_us = (int64_t)pps_utc_sec_out * 1000000LL;
    if (llabs(candidate_us - current_us) > kGpsCandidateJumpGuardUs) {
      return false;
    }
  }

  // Анти-±1с защита: сверяемся с текущей оценкой якоря, но только если якорь свежий
  bool anchor_fresh = false;
  if (s_status.source == TimeSource::GPS_PPS && s_status.last_pps_timestamp_us != 0) {
    int64_t now_us = esp_timer_get_time();
    anchor_fresh = (now_us - s_status.last_pps_timestamp_us) < kAnchorFreshnessUs;
  }

  if (anchor_fresh) {
    int64_t est_utc_us = 0;
    if (time_sync_esp_to_utc_us(pps_esp_us, est_utc_us)) {
      int64_t candidate_us = (int64_t)pps_utc_sec_out * 1000000LL;
      int64_t diff = candidate_us - est_utc_us;
      if (diff > 500000 && diff < 1500000) {
        pps_utc_sec_out -= 1;
      } else if (diff < -500000 && diff > -1500000) {
        pps_utc_sec_out += 1;
      }
    }
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

  const int64_t t0_us = esp_timer_get_time();
  int64_t  edge_us = 0;
  uint32_t edge_count = 0;

  // 1) Ждём ЛЮБОЙ фронт SQW
  while ((esp_timer_get_time() - t0_us) < (int64_t)timeout_ms * 1000LL) {
    int64_t  e_us = 0;
    uint32_t c = 0;
    if (rtc_sqw_get_raw(e_us, c)) {
      // edge "свежий"? (чтобы не взять старый)
      int64_t now_us = esp_timer_get_time();
      if ((now_us - e_us) >= 0 && (now_us - e_us) < 200000) { // <200ms
        edge_us = e_us;
        edge_count = c;
        break;
      }
    }
  // Небольшая пауза без blockinging
  esp_rom_delay_us(100);
  }

  if (edge_us == 0) {
    uint32_t sec0 = rtc.unixTime();
    timeval tv{ (time_t)sec0, 0 };
    settimeofday(&tv, nullptr);

    int64_t now_us = esp_timer_get_time();
    set_anchor(TimeSource::RTC, (int64_t)sec0 * 1000000LL, now_us);

    s_status.source = TimeSource::RTC;
    s_status.synced = true;
    s_status.last_sync_us = now_us;
    s_status.last_offset_us = 0;
    s_status.last_utc_second = sec0;

    ESP_LOGW(TAG, "Boot RTC sync: SQW edge timeout, using rtc=%lu", (unsigned long)sec0);
    return false;
  }

  // 2) На фронте читаем секунду RTC
  uint32_t rtc_sec = rtc.unixTime();

  // 3) Ставим ровно на границу
  timeval tv{ (time_t)rtc_sec, 0 };
  settimeofday(&tv, nullptr);

  // 4) Якорь + синхронизируем sqw-счётчики
  set_anchor(TimeSource::RTC, (int64_t)rtc_sec * 1000000LL, edge_us);
  s_last_sqw_count = edge_count;
  s_rtc_anchor_sqw_count = edge_count;
  s_last_sqw_edge_us = edge_us;
  s_last_rtc_resync_us = esp_timer_get_time();

  s_status.source = TimeSource::RTC;
  s_status.synced = true;
  s_status.last_sync_us = edge_us;
  s_status.last_offset_us = 0;
  s_status.last_utc_second = rtc_sec;

  ESP_LOGI(TAG, "Boot RTC sync: aligned to SQW edge (rtc=%lu, sqw_cnt=%lu)",
           (unsigned long)rtc_sec, (unsigned long)edge_count);

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

  s_last_rtc_resync_us = 0;
  s_last_rtc_pps_sync_us = 0;
  s_rtc_pps_pending = false;
  s_rtc_pps_target_sec = 0;
  s_rtc_pps_target_count = 0;

  s_last_sqw_count = 0;
  s_rtc_anchor_sqw_count = 0;
  s_last_sqw_edge_us = 0;
  s_have_rtc_anchor = false;
  rtc_sqw_begin(RTC_SQW_PIN, FALLING);

  s_in_rtc_fallback = false;
  s_logged_no_sqw = false;
  s_prev_sqw_locked = false;
  s_prev_have_sqw_edge = false;
  s_logged_sqw_warmup = false;
  s_logged_rtc_only = false;
  s_log_rtc_fallback_delta = false;
  s_rtc_fallback_guard_active = false;
  s_rtc_fallback_guard_logged = false;
  s_rtc_fallback_ticks = 0;
  s_logged_rtc_anchor_wait = false;
  s_gps_relock_guard_active = false;
  s_gps_relock_guard_logged = false;
  s_gps_relock_pps_ok = 0;
  reset_phase_delta_filter();

  // Инициализация системного времени по RTC
  if (rtc.isReady() && is_auto_sync_enabled()) {
    bool aligned = set_system_time_from_rtc_on_second_edge(1500);
    ESP_LOGI(TAG, "Boot time from RTC: %s", aligned ? "aligned to second" : "not aligned (timeout)");
  } else if (rtc.isReady()) {
    ESP_LOGI(TAG, "Auto sync disabled: boot time from RTC skipped");
  }

  notify_state_change_if_needed();
}

void time_sync_set_state_callback(TimeSyncStateCallback callback) {
  s_state_callback = callback;
  s_last_state = time_sync_state();
  if (s_state_callback)
    s_state_callback(s_last_state);
}

void time_sync_update() {
  const bool auto_sync_enabled = is_auto_sync_enabled();
  const uint8_t sync_source = settings.getSync().source;
  const bool allow_gps = (sync_source != 2);
  if (allow_gps) {
    s_logged_rtc_only = false;
  }

  // --- 1) NMEA: обновляем секунду и фиксируем момент её получения ---
  uint32_t gps_utc = 0;
  if (allow_gps && gps_time_to_unix(gps_utc)) {
    s_status.gps_time_valid = true;

    int64_t nmea_arrival_us = 0;
    if (!gps.lastSentenceStartUs(nmea_arrival_us)) {
      nmea_arrival_us = esp_timer_get_time();
    }

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
    s_have_nmea = false;
    reset_phase_delta_filter();
  }

  // --- 2) PPS lock? ---
  s_status.pps_locked = allow_gps && pps_is_locked();
  if (!s_status.pps_locked) {
    reset_phase_delta_filter();

    // PPS пропал -> отменяем запланированную установку RTC по "следующему PPS"
    s_rtc_pps_pending = false;
    s_gps_relock_guard_active = true;
    s_gps_relock_guard_logged = false;
    s_gps_relock_pps_ok = 0;
  }

  // --- 3) Если PPS нет — fallback на RTC (через SQW 1Hz) ---
  if (!s_status.pps_locked) {
    s_status.phase_aligned = false;

    if (!rtc.isReady()) {
      s_status.source = TimeSource::NONE;
      s_status.synced = false;
      notify_state_change_if_needed();
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
      notify_state_change_if_needed();
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
      notify_state_change_if_needed();
      return;
    }

    // c) SQW locked -> RTC OK
    s_logged_no_sqw = false;
    s_logged_sqw_warmup = false;

    // Переход в RTC fallback (один раз)
    if (!s_in_rtc_fallback) {
      s_in_rtc_fallback = true;
      s_log_rtc_fallback_delta = true;
      s_rtc_fallback_guard_active = true;
      s_rtc_fallback_guard_logged = false;
      s_rtc_fallback_ticks = 0;
      s_logged_rtc_anchor_wait = false;

      // Для красивого лога вытащим секунду RTC
      uint32_t rtc_sec = rtc.unixTime();
      if (allow_gps) {
        ESP_LOGW(TAG, "GPS/PPS lost -> RTC+SQW fallback. rtc=%lu", (unsigned long)rtc_sec);
      } else if (!s_logged_rtc_only) {
        ESP_LOGI(TAG, "RTC-only mode (sync.source=2). rtc=%lu", (unsigned long)rtc_sec);
        s_logged_rtc_only = true;
      }
    }

    // реагируем только на новый фронт SQW
    if (sqw_count != s_last_sqw_count) {
      s_last_sqw_count = sqw_count;
      s_last_sqw_edge_us = sqw_edge_us;
      if (s_rtc_fallback_ticks < 255) s_rtc_fallback_ticks++;

      int64_t now_us = esp_timer_get_time();
      int64_t age_us = now_us - sqw_edge_us;
      if (age_us < 0) age_us = 0;

      // если слишком поздно обработали тик — можно промахнуться на секунду
      if (age_us <= kSqwAgeWindowUs) {

        // -----------------------------------------------------------------------
        // 1) Переякоривание по rtc.unixTime() РАЗ В kRtcResyncPeriodUs (10 сек)
        // -----------------------------------------------------------------------
        const bool have_rtc_anchor =
            s_have_rtc_anchor && (s_status.anchor_utc_us != 0) && (s_status.anchor_esp_us != 0);

        bool need_reanchor =
            !have_rtc_anchor ||
            (s_rtc_anchor_sqw_count == 0) ||         // <-- важно: иначе d может улететь
            (s_last_rtc_resync_us == 0) ||
            ((now_us - s_last_rtc_resync_us) >= kRtcResyncPeriodUs);


        bool allow_reanchor_now = true;
        if (s_rtc_fallback_guard_active &&
            s_rtc_fallback_ticks <= kRtcFallbackWarmupTicks &&
            age_us > kRtcFallbackInitialAnchorMaxAgeUs) {
          allow_reanchor_now = false;
        }

        if (need_reanchor && !allow_reanchor_now) {
          if (!s_logged_rtc_anchor_wait) {
            ESP_LOGW(TAG, "RTC fallback: waiting reanchor window (tick=%u, age_us=%lld)",
                     (unsigned)s_rtc_fallback_ticks, (long long)age_us);
            s_logged_rtc_anchor_wait = true;
          }
          s_status.source = TimeSource::RTC;
          s_status.synced = true;
          notify_state_change_if_needed();
          return;
        }

        if (need_reanchor && allow_reanchor_now) {
          uint32_t rtc_sec = rtc.unixTime();
          int64_t rtc_utc_us = (int64_t)rtc_sec * 1000000LL;

          // ---- анти-±1 сек защита (оставляем твою) ----
          int64_t est_utc_us = 0;
          if (time_sync_esp_to_utc_us(sqw_edge_us, est_utc_us)) {
            int64_t diff = rtc_utc_us - est_utc_us;
            if (diff > 500000 && diff < 1500000)            rtc_utc_us -= 1000000LL;
            else if (diff < -500000 && diff > -1500000)     rtc_utc_us += 1000000LL;
          }

          set_anchor(TimeSource::RTC, rtc_utc_us, sqw_edge_us);
          s_rtc_anchor_sqw_count = sqw_count;   // важно: привязали якорь к этому sqw_count
          s_last_rtc_resync_us = now_us;
          s_logged_rtc_anchor_wait = false;
        }

        const bool have_valid_rtc_anchor =
            s_have_rtc_anchor &&
            (s_status.anchor_utc_us != 0) &&
            (s_status.anchor_esp_us != 0) &&
            (s_rtc_anchor_sqw_count != 0);

        if (!have_valid_rtc_anchor) {
          if (!s_logged_rtc_anchor_wait) {
            ESP_LOGW(TAG, "RTC fallback: waiting valid RTC anchor (tick=%u, age_us=%lld)",
                     (unsigned)s_rtc_fallback_ticks, (long long)age_us);
            s_logged_rtc_anchor_wait = true;
          }
          s_status.source = TimeSource::RTC;
          s_status.synced = true;
          notify_state_change_if_needed();
          return;
        }

        // -----------------------------------------------------------------------
        // 2) На КАЖДЫЙ SQW тик строим UTC по счётчику (без I2C)
        //    edge_utc = anchor_utc + (sqw_count - anchor_sqw_count)*1s
        // -----------------------------------------------------------------------
        int32_t d = (int32_t)(sqw_count - s_rtc_anchor_sqw_count); // signed delta
        int64_t edge_utc_us = s_status.anchor_utc_us + (int64_t)d * 1000000LL;

        // цель "сейчас": момент edge + возраст обработки
        int64_t target_us = edge_utc_us + age_us;

        // -----------------------------------------------------------------------
        // 3) Дисциплина системного времени (каждую секунду) — как было
        // -----------------------------------------------------------------------
        timeval current_tv{};
        gettimeofday(&current_tv, nullptr);
        int64_t current_us =
            (int64_t)current_tv.tv_sec * 1000000LL + (int64_t)current_tv.tv_usec;

        int64_t delta_us = target_us - current_us;

        bool need_adjust = !s_status.synced ||
                          (delta_us < -kJitterAdjustThresholdUs || delta_us > kJitterAdjustThresholdUs) ||
                          ((now_us - s_status.last_sync_us) > kMaxHoldoffUs);

        if (s_log_rtc_fallback_delta) {
          ESP_LOGI(TAG, "RTC fallback initial delta_us=%lld (target=%lld, current=%lld, age_us=%lld)",
                   (long long)delta_us, (long long)target_us, (long long)current_us, (long long)age_us);
          s_log_rtc_fallback_delta = false;
        }

        bool allow_adjust = true;
        if (s_rtc_fallback_guard_active) {
          if (s_rtc_fallback_ticks < kRtcFallbackWarmupTicks) {
            allow_adjust = false;
          } else if (llabs(delta_us) > kRtcFallbackLargeStepGuardUs) {
            allow_adjust = false;
            if (!s_rtc_fallback_guard_logged) {
              ESP_LOGW(TAG,
                       "RTC fallback guard: skip large delta_us=%lld (tick=%u, age_us=%lld)",
                       (long long)delta_us,
                       (unsigned)s_rtc_fallback_ticks,
                       (long long)age_us);
              s_rtc_fallback_guard_logged = true;
            }
          } else {
            s_rtc_fallback_guard_active = false;
            s_rtc_fallback_guard_logged = false;
            ESP_LOGI(TAG,
                     "RTC fallback guard cleared: delta_us=%lld after %u SQW ticks",
                     (long long)delta_us,
                     (unsigned)s_rtc_fallback_ticks);
          }
        }

        if (need_adjust && allow_adjust) {
          if (auto_sync_enabled) {
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
          } else {
            s_status.last_offset_us = delta_us;
          }
        } else if (!allow_adjust) {
          s_status.last_offset_us = delta_us;
        }
      }
    }

    s_status.source = TimeSource::RTC;
    s_status.synced = true;
    notify_state_change_if_needed();
    return;
  }

  // --- 4) PPS есть: берём raw PPS ---
  int64_t  pps_time_us = 0;
  uint32_t pps_count   = 0;
  if (!pps_get_raw(pps_time_us, pps_count)) {
    s_status.phase_aligned = false;
    notify_state_change_if_needed();
    return;
  }

  // Реагируем только на новый импульс
  if (pps_count == s_last_pps_count) {
    notify_state_change_if_needed();
    return;
  }
  s_last_pps_count = pps_count;

  // Уже синхронизировали этот PPS (доп. защита)
  if (pps_time_us == s_last_synced_pps_us) {
    notify_state_change_if_needed();
    return;
  }

  // --- 5) PPS↔NMEA phase alignment ---
  uint32_t utc_second = 0;
  int64_t  phase_delta_us = 0;
  if (!align_pps_utc(pps_time_us, utc_second, phase_delta_us) || utc_second == 0) {
    s_status.phase_aligned = false;
    s_status.last_phase_delta_us = phase_delta_us;

    ESP_LOGW(TAG, "PPS received but cannot align with NMEA (delta=%lld us, have_nmea=%d)",
             (long long)phase_delta_us, (int)s_have_nmea);
    notify_state_change_if_needed();
    return;
  }

  s_status.phase_aligned = true;
  s_status.last_phase_delta_us = phase_delta_us;

  // GPS режим: якорь = точный PPS
  set_anchor(TimeSource::GPS_PPS, (int64_t)utc_second * 1000000LL, pps_time_us);

  // Возврат из RTC fallback (один раз)
  if (s_in_rtc_fallback) {
    s_in_rtc_fallback = false;
    s_rtc_fallback_guard_active = false;
    s_rtc_fallback_guard_logged = false;
    s_rtc_fallback_ticks = 0;
    s_logged_rtc_anchor_wait = false;
    ESP_LOGI(TAG, "GPS/PPS restored -> GPS_PPS mode");
  }

  // --- 6) (Опционально) дисциплинируем системные часы через settimeofday ---
  int64_t now_us = esp_timer_get_time();
  int64_t age_us = now_us - pps_time_us;
  if (age_us < 0) age_us = 0;

  // Дисциплина RTC по PPS: отложенная установка на следующий PPS
  if (s_rtc_pps_pending && pps_count == s_rtc_pps_target_count) {
    if (auto_sync_enabled && age_us <= kRtcPpsAlignWindowUs && rtc.isReady()) {
      rtc.setTime(s_rtc_pps_target_sec);
      ESP_LOGI(TAG, "RTC set from PPS: %lu (aligned, age %lld us)",
               (unsigned long)s_rtc_pps_target_sec, (long long)age_us);
      s_last_rtc_pps_sync_us = now_us;
      s_rtc_synced = true;
    }
    s_rtc_pps_pending = false;
  }

  // rtc.setTime() может занять миллисекунды (I2C), поэтому пересчитываем
  // "сейчас" и возраст PPS перед дисциплиной системных часов.
  now_us = esp_timer_get_time();
  age_us = now_us - pps_time_us;
  if (age_us < 0) age_us = 0;

  int64_t target_us = (int64_t)utc_second * 1000000LL + age_us;

  timeval current_tv{};
  gettimeofday(&current_tv, nullptr);
  int64_t current_us = (int64_t)current_tv.tv_sec * 1000000LL + (int64_t)current_tv.tv_usec;

  int64_t delta_us = target_us - current_us;

  if (s_gps_relock_guard_active && s_gps_relock_pps_ok < 255) {
    s_gps_relock_pps_ok++;
  }

  bool need_adjust = !s_status.synced ||
                     (delta_us < -kJitterAdjustThresholdUs || delta_us > kJitterAdjustThresholdUs) ||
                     ((now_us - s_status.last_sync_us) > kMaxHoldoffUs);

  bool allow_gps_adjust = true;
  if (s_gps_relock_guard_active) {
    if (s_gps_relock_pps_ok < kGpsRelockWarmupPps) {
      allow_gps_adjust = false;
    } else if (llabs(delta_us) > kGpsRelockLargeStepGuardUs) {
      allow_gps_adjust = false;
      if (!s_gps_relock_guard_logged) {
        ESP_LOGW(TAG,
                 "GPS relock guard: skip large delta_us=%lld (pps_ok=%u, age_us=%lld)",
                 (long long)delta_us,
                 (unsigned)s_gps_relock_pps_ok,
                 (long long)age_us);
        s_gps_relock_guard_logged = true;
      }
    } else {
      s_gps_relock_guard_active = false;
      s_gps_relock_guard_logged = false;
      ESP_LOGI(TAG,
               "GPS relock guard cleared: delta_us=%lld after %u PPS",
               (long long)delta_us,
               (unsigned)s_gps_relock_pps_ok);
    }
  }

  if (need_adjust && allow_gps_adjust) {
    if (auto_sync_enabled) {
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
    }
    s_status.last_offset_us = delta_us;
  } else if (!allow_gps_adjust) {
    s_status.last_offset_us = delta_us;
  }

  s_status.synced                = true;
  s_status.last_pps_timestamp_us = pps_time_us;
  s_status.last_offset_us        = delta_us;
  s_status.last_utc_second       = utc_second;

  // Планируем следующее дисциплинирование RTC по PPS
  if (!s_rtc_pps_pending &&
      s_status.phase_aligned &&
      s_status.gps_time_valid &&
      // первая фронтовая синхронизация RTC будет сразу при первом phase_aligned (PPS+NMEA),
      // а дальше через kRtcPpsSyncPeriodUs.
      auto_sync_enabled &&
      (s_last_rtc_pps_sync_us == 0 ||
       (now_us - s_last_rtc_pps_sync_us) >= kRtcPpsSyncPeriodUs)) {
    s_rtc_pps_pending = true;
    s_rtc_pps_target_sec = utc_second + 1;
    s_rtc_pps_target_count = pps_count + 1;
  }

  notify_state_change_if_needed();
}

TimeSyncStatus time_sync_status() { return s_status; }

TimeSyncState time_sync_state() {
  if (!s_status.synced) return TimeSyncState::NONE;

  // PPS есть — это всегда GPS-* состояние, даже если anchor пока RTC
  if (s_status.pps_locked) {
    return s_status.phase_aligned ? TimeSyncState::GPS_OK : TimeSyncState::GPS_DEGRADED;
  }

  if (s_status.source == TimeSource::RTC) {
    return rtc_sqw_is_locked() ? TimeSyncState::RTC_OK : TimeSyncState::RTC_DEGRADED;
  }

  return TimeSyncState::NONE;
}

int64_t time_sync_estimate_accuracy_us() {
  if (!s_status.synced)
    return -1;

  const bool auto_sync = is_auto_sync_enabled();
  const int64_t now_us = esp_timer_get_time();

  switch (s_status.source) {

    case TimeSource::GPS_PPS: {
      // Без phase alignment мы не знаем, какая UTC секунда у PPS
      if (!s_status.pps_locked || !s_status.phase_aligned)
        return -1;

      // База: ISR/таймер + остаток phase модели
      int64_t acc = kPpsIsrJitterUs + kGpsPhaseResidualUs;

      // "Свежесть" PPS якоря: esp_timer дрейфует, даём мягкий штраф
      int64_t age_us = now_us - s_status.last_pps_timestamp_us;
      if (age_us < 0) age_us = 0;

      // допустим дрейф ~1..2 us/сек (консервативно 2 us/сек)
      acc += (age_us / 1000000LL) * 2;

      // Если авто-дисциплина выключена — системные часы могут быть с большим offset
      // Тогда accuracy должна честно это показать.
      if (!auto_sync) {
        acc += llabs(s_status.last_offset_us);
      }

      return acc;
    }

    case TimeSource::RTC: {
      if (!rtc_sqw_is_locked())
        return -1;

      // База: SQW ISR/latency + (редкое) чтение RTC при реякоре
      int64_t acc = kRtcBaseJitterUs;

      // Если авто-дисциплина выключена — добавляем реальный offset
      if (!auto_sync) {
        acc += llabs(s_status.last_offset_us);
        return acc;
      }

      // Мягкое ухудшение, если давно не было актуального SQW edge/anchor обновления
      // (например loop сильно тормозит). Не "1ms за секунду", а аккуратнее и с потолком.
      int64_t age_us = (s_last_sqw_edge_us != 0) ? (now_us - s_last_sqw_edge_us)
                                                 : (now_us - s_status.anchor_esp_us);
      if (age_us < 0) age_us = 0;

      if (age_us > 1000000LL) {
        // +1ms за каждые 5 секунд "просрочки" после первой секунды
        int64_t extra_ms = (age_us - 1000000LL) / 5000000LL;
        int64_t extra_us = extra_ms * 1000LL;

        if (extra_us > kRtcAgingCapUs) extra_us = kRtcAgingCapUs;
        acc += extra_us;
      }

      return acc;
    }

    default:
      return -1;
  }
}
