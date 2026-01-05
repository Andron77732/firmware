#include "event_timestamp.h"
#include "esp_log.h"
#include "hal/comm/ble.h"
#include "storage/settings.h"
#include "time_sync.h"
#include <time.h>

static const char *TAG = "EVENT_TS";

EventTimestampData event_timestamp_process(int64_t esp_timestamp_us,
                                           ModuleType module_type) {
  EventTimestampData data{};
  data.esp_timestamp_us = esp_timestamp_us;
  data.module_type = module_type;
  data.success = false;
  snprintf(data.local_time_str, sizeof(data.local_time_str), "--:--:--,---");

  int64_t t_utc_us = 0;

  // Конвертация ESP timestamp в UTC
  if (!time_sync_esp_to_utc_us(esp_timestamp_us, t_utc_us)) {
    // GPS и RTC ещё не готовы
    ESP_LOGW(TAG, "EVENT esp = %lld us (no time source)",
             (long long)esp_timestamp_us);
    return data; // Возвращаем структуру с success=false
  }

  data.utc_timestamp_us = t_utc_us;
  data.success = true;

  // Вывод UTC времени
  ESP_LOGD(TAG, "EVENT UTC = %lld us", (long long)t_utc_us);

  // Вычисление локального времени в формате hh:mm:ss,mmm
  int8_t timezone = settings.getDevice().timezone;
  int64_t t_local_us = t_utc_us + ((int64_t)timezone * 3600LL * 1000000LL);

  // Сохраняем локальное время в структуре timeval
  data.local_time.tv_sec = (time_t)(t_local_us / 1000000LL);
  int64_t local_usec = t_local_us % 1000000LL;
  // Обработка отрицательного остатка
  if (local_usec < 0) {
    local_usec += 1000000LL;
    data.local_time.tv_sec -= 1;
  }
  data.local_time.tv_usec = (suseconds_t)local_usec;

  // Извлечение отдельных компонентов времени
  struct tm tm{};
  gmtime_r(&data.local_time.tv_sec, &tm);
  data.hours = tm.tm_hour;
  data.minutes = tm.tm_min;
  data.seconds = tm.tm_sec;
  data.milliseconds = (uint16_t)(data.local_time.tv_usec / 1000);

  // Форматирование строки времени "HH:MM:SS,mmm" из компонентов
  snprintf(data.local_time_str, sizeof(data.local_time_str), "%02d:%02d:%02d,%03d", data.hours, data.minutes, data.seconds, data.milliseconds);

  // Вывод локального времени в зависимости от типа модуля
  if (data.module_type == ModuleType::START) {
    ESP_LOGI(TAG, "START EVENT LOCAL = %s", data.local_time_str);
  } else {
    ESP_LOGI(TAG, "FINISH EVENT LOCAL = %s", data.local_time_str);
  }

  return data;
}

void event_timestamp_send_ble(const EventTimestampData &data) {
  // Заглушка: только логирование, без реальной отправки
  if (!bleSerial.isConnected()) {
    ESP_LOGD(TAG, "BLE not connected, skipping event send");
    return;
  }

  // Получаем тип модуля из структуры данных
  const char *event_type = (data.module_type == ModuleType::START) ? "START" : "FINISH";

  if (data.success) {
    ESP_LOGI(TAG, "[BLE STUB] %s EVENT: UTC=%lld us, LOCAL=%s", event_type,
             (long long)data.utc_timestamp_us, data.local_time_str);
  } else {
    ESP_LOGI(TAG, "[BLE STUB] %s EVENT: esp=%lld us (no time source)",
             event_type, (long long)data.esp_timestamp_us);
  }
}
