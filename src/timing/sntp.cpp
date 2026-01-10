#include "sntp.h"

#include <time.h>
#include <esp_sntp.h>

static const char *TAG = "SNTP";

static bool waitForSntpSync(uint32_t timeoutMs)
{
  const uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) return true;
    delay(SNTP_POLL_DELAY_MS);
  }
  return false;
}

static bool writeRtcOnSecondEdgeUtc(RTC &rtc,
                                   uint32_t edgeWindowUs,
                                   uint32_t timeoutMs)
{
  struct timeval tv{};
  time_t lastSec = (time_t)-1;

  const uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    gettimeofday(&tv, nullptr);

    // Поймать начало новой секунды и попасть в маленькое окно в начале секунды
    if (tv.tv_sec != lastSec && (uint32_t)tv.tv_usec <= edgeWindowUs) {
      rtc.setTime((uint32_t)tv.tv_sec); // записываем UTC seconds
      return true;
    }

    lastSec = tv.tv_sec;

    // Не грузим CPU
    delay(SNTP_EDGE_POLL_DELAY_MS);
  }

  return false;
}

bool syncRtcUtcFromNtpPrecise(RTC &rtc,
                              WiFiManager &wifiManager,
                              const char *ntp1,
                              const char *ntp2,
                              uint32_t timeoutSyncMs,
                              uint32_t timeoutEdgeMs,
                              uint32_t edgeWindowUs,
                              uint32_t *outDurationMs)
{
  if (outDurationMs) {
    *outDurationMs = 0;
  }

  if (!wifiManager.isConnected()) {
    ESP_LOGW(TAG, "WiFi not connected");
    return false;
  }

  if (!rtc.isReady()) {
    ESP_LOGW(TAG, "RTC not found");
    return false;
  }

  // Сбросим статус, чтобы COMPLETED был именно от текущей попытки
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);

  // UTC: смещение 0, DST 0
  const uint32_t t0 = millis();
  if (ntp2 && ntp2[0]) configTime(0, 0, ntp1, ntp2);
  else                 configTime(0, 0, ntp1);

  if (!waitForSntpSync(timeoutSyncMs)) {
    ESP_LOGW(TAG, "NTP sync timeout");
    return false;
  }

  if (!writeRtcOnSecondEdgeUtc(rtc, edgeWindowUs, timeoutEdgeMs)) {
    ESP_LOGW(TAG, "RTC edge-align timeout");
    return false;
  }

  if (outDurationMs) {
    *outDurationMs = millis() - t0;
  }

  // Контрольный лог (можешь убрать)
  DateTime r = rtc.now();
  ESP_LOGI(TAG, "RTC set (UTC): %04d-%02d-%02d %02d:%02d:%02d",
                r.year(), r.month(), r.day(), r.hour(), r.minute(), r.second());

  return true;
}
