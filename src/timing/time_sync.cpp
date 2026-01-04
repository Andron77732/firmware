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
static uint32_t       s_last_gps_utc = 0;
static bool           s_rtc_synced = false;

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

  if (year < 2020 || month == 0 || day == 0 || hour > 23 || minute > 59 ||
      second > 60)
    return false;

  DateTime dt(year, month, day, hour, minute, second);
  unix_sec = dt.unixtime();
  return unix_sec > 0;
}

void time_sync_begin() { s_status = TimeSyncStatus{}; }

void time_sync_update() {
  // Обновляем UTC секунду из GPS (каждый раз, когда есть валидное время)
  uint32_t gps_utc = 0;
  if (gps_time_to_unix(gps_utc)) {
    s_status.gps_time_valid = true;
    if (gps_utc != s_last_gps_utc) {
      pps_set_utc_second(gps_utc);
      s_last_gps_utc = gps_utc;
    }
  } else {
    s_status.gps_time_valid = false;
  }

  s_status.pps_locked = pps_is_locked();
  if (!s_status.pps_locked)
    return;

  int64_t  pps_time_us = 0;
  uint32_t utc_second  = 0;
  if (!pps_get(pps_time_us, utc_second) || utc_second == 0)
    return;

  // Уже синхронизировали эту PPS
  if (pps_time_us == s_last_synced_pps_us)
    return;

  int64_t now_us  = esp_timer_get_time();
  int64_t age_us  = now_us - pps_time_us;
  if (age_us < 0)
    age_us = 0;

  time_t sec  = utc_second + static_cast<uint32_t>(age_us / 1000000);
  suseconds_t usec = static_cast<suseconds_t>(age_us % 1000000);

  timeval tv{.tv_sec = sec, .tv_usec = usec};
  settimeofday(&tv, nullptr);

  s_status.synced               = true;
  s_status.last_sync_us         = now_us;
  s_status.last_pps_timestamp_us = pps_time_us;
  s_status.last_utc_second      = static_cast<uint32_t>(sec);
  s_status.last_offset_us       = age_us;
  s_last_synced_pps_us          = pps_time_us;

  ESP_LOGI(TAG,
           "System time synced: %ld.%06ld (age %lld us, utc %lu, pps %lld)",
           static_cast<long>(tv.tv_sec), static_cast<long>(tv.tv_usec),
           static_cast<long long>(age_us), static_cast<unsigned long>(utc_second),
           static_cast<long long>(pps_time_us));

  // Обновляем RTC один раз после первой синхронизации
  if (!s_rtc_synced && rtc.isReady()) {
    rtc.setTime(static_cast<uint32_t>(tv.tv_sec));
    s_rtc_synced = true;
  }
}

TimeSyncStatus time_sync_status() { return s_status; }
