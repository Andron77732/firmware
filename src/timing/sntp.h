#pragma once

#include "config.h"
#include "hal/wifi/wifi.h"
#include "hal/rtc/rtc.h"
#include <Arduino.h>

/**
 * @brief Точная синхронизация RTC (UTC) по NTP с выравниванием на границу секунды
 * 
 * Выполняет трёхэтапную синхронизацию:
 * 1) Запускает SNTP клиент (configTime) с указанными NTP серверами
 * 2) Ожидает успешной синхронизации SNTP (SNTP_SYNC_STATUS_COMPLETED)
 * 3) Записывает время в RTC на границе секунды (в пределах edgeWindowUs)
 * 
 * @param rtc Ссылка на объект RTC для записи времени
 * @param wifiManager Менеджер WiFi (должен быть подключён)
 * @param ntp1 Первичный NTP сервер
 * @param ntp2 Вторичный NTP сервер, опционально
 * @param ntp3 Третичный NTP сервер, опционально
 * @param timeoutSyncMs Таймаут ожидания SNTP синхронизации, мс (по умолчанию: SNTP_SYNC_TIMEOUT_MS)
 * @param timeoutEdgeMs Таймаут ожидания границы секунды, мс (по умолчанию: SNTP_EDGE_TIMEOUT_MS)
 * @param edgeWindowUs Окно для записи RTC в начале секунды, мкс (по умолчанию: SNTP_EDGE_WINDOW_US)
 * @param outDurationMs Указатель для возврата длительности синхронизации, мс (опционально)
 * 
 * @return true если синхронизация успешна, false при ошибке или таймауте
 * 
 * @note Требования:
 *       - WiFi должен быть подключён (wifiManager.isConnected() == true)
 *       - RTC должен быть инициализирован и готов (rtc.isReady() == true)
 * 
 * @note Время записывается в RTC в формате UTC (без часового пояса)
 */
bool syncRtcUtcFromNtpPrecise(
    RTC &rtc,
    WiFiManager &wifiManager,
    const char *ntp1,
    const char *ntp2,
    const char *ntp3,
    uint32_t timeoutSyncMs = SNTP_SYNC_TIMEOUT_MS,
    uint32_t timeoutEdgeMs = SNTP_EDGE_TIMEOUT_MS,
    uint32_t edgeWindowUs  = SNTP_EDGE_WINDOW_US,
    uint32_t *outDurationMs = nullptr
);
