#include "event_timestamp.h"
#include "time_sync.h"
#include "storage/settings.h"
#include "hal/comm/ble.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "EVENT_TS";

EventTimestampData event_timestamp_process(int64_t esp_timestamp_us, ModuleType module_type) {
  EventTimestampData data{};
  data.esp_timestamp_us = esp_timestamp_us;
  data.success = false;
  
  int64_t t_utc_us = 0;
  
  // Конвертация ESP timestamp в UTC
  if (!time_sync_esp_to_utc_us(esp_timestamp_us, t_utc_us)) {
    // GPS и RTC ещё не готовы
    if (module_type == ModuleType::START) {
      ESP_LOGW(TAG, "START EVENT esp = %lld us (no time source)",
               (long long)esp_timestamp_us);
    } else {
      ESP_LOGW(TAG, "FINISH EVENT esp = %lld us (no time source)",
               (long long)esp_timestamp_us);
    }
    return data; // Возвращаем структуру с success=false
  }

  data.utc_timestamp_us = t_utc_us;
  data.success = true;

  // Вывод UTC времени в зависимости от типа модуля
  if (module_type == ModuleType::START) {
    ESP_LOGI(TAG, "START EVENT UTC = %lld us", (long long)t_utc_us);
  } else {
    ESP_LOGI(TAG, "FINISH EVENT UTC = %lld us", (long long)t_utc_us);
  }

  // Вычисление локального времени в формате hh:mm:ss,mmm
  int8_t timezone = settings.getDevice().timezone;
  int64_t t_local_us = t_utc_us + ((int64_t)timezone * 3600LL * 1000000LL);

  time_t local_sec = (time_t)(t_local_us / 1000000LL);
  int64_t local_usec = t_local_us % 1000000LL;
  // Обработка отрицательного остатка
  if (local_usec < 0) {
    local_usec += 1000000LL;
    local_sec -= 1;
  }
  int local_msec = (int)(local_usec / 1000);

  struct tm tm{};
  gmtime_r(&local_sec, &tm);
  data.hour = static_cast<uint8_t>(tm.tm_hour);
  data.minute = static_cast<uint8_t>(tm.tm_min);
  data.second = static_cast<uint8_t>(tm.tm_sec);
  data.millisecond = static_cast<uint16_t>(local_msec);

  // Форматирование строки времени "HH:MM:SS,mmm"
  char time_str[13]; // "HH:MM:SS,mmm\0"
  snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d,%03d",
           data.hour, data.minute, data.second, data.millisecond);
  data.local_time_str = String(time_str);

  // Вывод локального времени в зависимости от типа модуля
  if (module_type == ModuleType::START) {
    ESP_LOGI(TAG, "START EVENT LOCAL = %s", data.local_time_str.c_str());
  } else {
    ESP_LOGI(TAG, "FINISH EVENT LOCAL = %s", data.local_time_str.c_str());
  }

  return data;
}

void event_timestamp_send_ble(const EventTimestampData& data) {
  // Заглушка: только логирование, без реальной отправки
  if (!bleSerial.isConnected()) {
    ESP_LOGD(TAG, "BLE not connected, skipping event send");
    return;
  }

  // Получаем тип модуля из settings для логирования
  uint8_t device_type = settings.getDevice().type;
  const char* event_type = (device_type == 1) ? "START" : "FINISH";

  if (data.success) {
    ESP_LOGI(TAG, "[BLE STUB] %s EVENT: UTC=%lld us, LOCAL=%s",
             event_type, (long long)data.utc_timestamp_us, data.local_time_str.c_str());
  } else {
    ESP_LOGI(TAG, "[BLE STUB] %s EVENT: esp=%lld us (no time source)",
             event_type, (long long)data.esp_timestamp_us);
  }
}

