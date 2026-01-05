#include "config.h"
#include "esp_log.h"
#include "hal/comm/ble.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "hal/tft/tft.h"
#include "storage/settings.h"
#include "timing/event_isr.h"
#include "timing/pps_isr.h"
#include "timing/time_sync.h"
#include "ui/status_bar.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <time.h>
#include <string>
#include <cstring>

static const char *TAG = "MAIN";

static ModuleType module_type = ModuleType::START; // значение по умолчанию

/**
 * @brief Callback для обновления иконки Bluetooth при изменении состояния BLE
 * @param state Текущее состояние Bluetooth
 */
void onBLEStateChanged(BLEState state) { statusBar.updateBluetoothIcon(state); }

/**
 * @brief Обновление статус-бара и связанных элементов (когда меняется секунда в системном времени)
 */
void updateStatusBar()
{
  static uint8_t lastSecond = 255;

  // Предпочитаем системное время (синхронизируется по GPS PPS)
  time_t nowSec = time(nullptr);

  if (nowSec <= 0)
    return; // Время ещё не установлено

  // Получаем таймзону из настроек и применяем смещение к UTC времени
  int8_t timezone = settings.getDevice().timezone;
  time_t local_time = nowSec + (timezone * 3600);

  struct tm tm{};
  gmtime_r(&local_time, &tm);
  uint8_t second = static_cast<uint8_t>(tm.tm_sec);

  // Обновляем только когда секунда изменилась в системном времени
  if (second != lastSecond)
  {
    lastSecond = second;

    uint8_t hour = static_cast<uint8_t>(tm.tm_hour);
    uint8_t minute = static_cast<uint8_t>(tm.tm_min);

    // Время в статус-баре
    statusBar.updateTime(hour, minute, second);

    // Статус под статус-баром: спутники + источник времени
    uint8_t sats = gps.isReady() ? gps.nmea().getNumSatellites() : 0;
    TimeSyncStatus ts = time_sync_status();
    const char *src = "NOSYNC";
    if (ts.source == TimeSource::GPS_PPS)
      src = "GPS   ";
    else if (ts.source == TimeSource::RTC)
      src = "RTC   ";

    display.tft().setCursor(0, StatusBar::HEIGHT + 60);
    display.tft().setTextSize(2);
    display.tft().setTextColor(TFT_CYAN, TFT_BLACK);
    display.tft().printf("Sats:%02u  Sync:%s", sats, src);
  }
}

void setup()
{
  Serial.begin(SERIAL_BAUD);

  ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);

  // Инициализация настроек (загрузка из NVS)
  if (!settings.begin()) {
    ESP_LOGE(TAG, "Failed to initialize settings manager");
    // Продолжаем работу с настройками по умолчанию
  }

  // Инициализация типа модуля из настроек
  {
    const DeviceSettings& device = settings.getDevice();
    const char* device_type_cstr = device.type.c_str();
    if (strcmp(device_type_cstr, "start") == 0) {
      module_type = ModuleType::START;
    } else if (strcmp(device_type_cstr, "finish") == 0) {
      module_type = ModuleType::FINISH;
    } else {
      ESP_LOGW(TAG, "Unknown device type '%s', defaulting to START", device_type_cstr);
      module_type = ModuleType::START;
    }
    ESP_LOGI(TAG, "Module type: %s", device_type_cstr);
  }

  // Инициализация прерывания на событие
  event_isr_init(EXT_INT_PIN);
  ESP_LOGI(TAG, "Event ISR initialized");

  // Инициализация GPS
  gps.begin();

  // Инициализация RTC
  rtc.begin();

  // PPS синхронизация от GPS
  pps_init(GPS_PPS_PIN);

  // Инициализация подсистемы синхронизации времени
  time_sync_begin();

  // Инициализация дисплея
  display.begin();

  // Инициализация статус-бара
  statusBar.begin(display.tft());
  statusBar.draw();

  // Заголовок под статус-баром
  display.tft().setCursor(0, StatusBar::HEIGHT + 10);
  display.tft().setTextSize(2);
  display.tft().setTextColor(TFT_WHITE, TFT_BLACK);
  display.tft().printf("ENTime v%s", VERSION);

  display.tft().setCursor(0, StatusBar::HEIGHT + 35);
  display.tft().setTextSize(1);
  display.tft().setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.tft().print("GPS Time Sync");

  // Инициализация BLE (Nordic UART Service)
  bleSerial.begin("ENTime");

  // Установка callback для мгновенного обновления иконки Bluetooth при
  // изменении состояния
  bleSerial.setStateCallback(onBLEStateChanged);

  // Небольшая задержка для того, чтобы реклама успела запуститься
  delay(50);

  // Обновляем иконку Bluetooth до текущего состояния (реклама уже запущена)
  bleSerial.notifyStateChanged();

  ESP_LOGI(TAG, "Setup complete");
}

void loop()
{
  gps.update();
  time_sync_update();

  // Обработка BLE данных
  if (bleSerial.available())
  {
    String data = bleSerial.readString();
    ESP_LOGD(TAG, "BLE RX: %s", data.c_str());
  }

  // Проверка события и вывод его UTC времени
  int64_t t_esp_us = 0;

  if (event_isr_get(t_esp_us))
  {
    int64_t t_utc_us = 0;
    if (time_sync_esp_to_utc_us(t_esp_us, t_utc_us))
    {
      // Вывод UTC времени в зависимости от типа модуля
      if (module_type == ModuleType::START) {
        ESP_LOGI(TAG, "START EVENT UTC = %lld us", (long long)t_utc_us);
      } else {
        ESP_LOGI(TAG, "FINISH EVENT UTC = %lld us", (long long)t_utc_us);
      }

      // Вывод локального времени в формате hh:mm:ss,sss
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
      uint8_t hour = static_cast<uint8_t>(tm.tm_hour);
      uint8_t minute = static_cast<uint8_t>(tm.tm_min);
      uint8_t second = static_cast<uint8_t>(tm.tm_sec);

      // Вывод локального времени в зависимости от типа модуля
      if (module_type == ModuleType::START) {
        ESP_LOGI(TAG, "START EVENT LOCAL = %02d:%02d:%02d,%03d", hour, minute, second, local_msec);
      } else {
        ESP_LOGI(TAG, "FINISH EVENT LOCAL = %02d:%02d:%02d,%03d", hour, minute, second, local_msec);
      }
    }
    else
    {
      // GPS и RTC ещё не готовы
      if (module_type == ModuleType::START) {
        ESP_LOGW(TAG, "START EVENT esp = %lld us (no time source)", (long long)t_esp_us);
      } else {
        ESP_LOGW(TAG, "FINISH EVENT esp = %lld us (no time source)", (long long)t_esp_us);
      }
    }
  }

  // Обновление статус-бара
  updateStatusBar();
}
