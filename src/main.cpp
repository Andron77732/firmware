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
#include "ui/footer.h"
#include "ui/main_area.h"
#include "ui/status_bar.h"
#include "ui/ui_config.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <time.h>

static const char *TAG = "MAIN";

static ModuleType module_type = ModuleType::START; // значение по умолчанию

/**
 * @brief Callback для обновления иконки Bluetooth при изменении состояния BLE
 * @param state Текущее состояние Bluetooth
 */
void onBLEStateChanged(BLEState state) { statusBar.updateBluetoothIcon(state); }

/**
 * @brief Обновление статус-бара и связанных элементов (когда меняется секунда в
 * системном времени)
 */
void updateStatusBar() {
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
  if (second != lastSecond) {
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

    display.tft().setCursor(0, UI_STATUS_BAR_HEIGHT + 60);
    display.tft().setTextSize(2);
    display.tft().setTextColor(TFT_CYAN, TFT_BLACK);
    display.tft().printf("Sats:%02u  Sync:%s", sats, src);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);

  // Инициализация дисплея (раньше, чтобы можно было показывать логи)
  display.begin();

  // Инициализация статус-бара
  statusBar.begin(display.tft());
  statusBar.draw();

  // Инициализация footer
  footer.begin(display.tft());

  // Инициализация mainArea в режиме загрузки
  mainArea.begin(display.tft());
  mainArea.setType(MainAreaType::LOADING);
  mainArea.draw();
  mainArea.addLogLine("ENTime v" VERSION " starting...");

  // Инициализация настроек (загрузка из NVS)
  mainArea.addLogLine("Loading settings...");
  if (!settings.begin()) {
    ESP_LOGE(TAG, "Failed to initialize settings manager");
    mainArea.addLogLine("ERROR: Settings init failed");
    mainArea.addLogLine("Default settings loaded");
    delay(2000);
    // Продолжаем работу с настройками по умолчанию
  } else {
    mainArea.addLogLine("Settings loaded");
  }

  // Инициализация типа модуля из настроек и формирование имени устройства
  char deviceName[32]; // Имя устройства для BLE (формат: "Имя-Номер")

  const DeviceSettings &device = settings.getDevice();
  if (device.type == 1) {
    module_type = ModuleType::START;
  } else if (device.type == 2) {
    module_type = ModuleType::FINISH;
  } else {
    ESP_LOGW(TAG, "Unknown device type %u, defaulting to START", device.type);
    module_type = ModuleType::START;
  }
  ESP_LOGI(TAG, "Module type: %u (%s)", device.type,
           module_type == ModuleType::START ? "START" : "FINISH");
  char moduleTypeStr[40];
  snprintf(moduleTypeStr, sizeof(moduleTypeStr), "Module type: %s",
           module_type == ModuleType::START ? "START" : "FINISH");
  mainArea.addLogLine(moduleTypeStr);

  // Инициализация прерывания на событие
  event_isr_init(EXT_INT_PIN);
  ESP_LOGI(TAG, "Event ISR initialized");

  // Инициализация GPS
  mainArea.addLogLine("Initializing GPS...");
  gps.begin();
  mainArea.addLogLine("GPS initialized");

  // Инициализация RTC
  mainArea.addLogLine("Initializing RTC...");
  if (rtc.begin()) {
    ESP_LOGI(TAG, "RTC initialized");
    mainArea.addLogLine("RTC initialized");
    // Проверка потери питания
    if (rtc.lostPower()) {
      ESP_LOGW(TAG, "RTC lost power");
      mainArea.addLogLine("WARN: RTC lost power");
      delay(2000);
    }
  } else {
    ESP_LOGE(TAG, "RTC init failed");
    mainArea.addLogLine("ERROR: RTC init failed");
    delay(5000);
  }

  // PPS синхронизация от GPS
  pps_init(GPS_PPS_PIN);
  ESP_LOGI(TAG, "PPS initialized");

  // Инициализация подсистемы синхронизации времени
  mainArea.addLogLine("Starting time sync...");
  time_sync_begin();
  ESP_LOGI(TAG, "Time sync ready");
  mainArea.addLogLine("Time sync ready");

  // Инициализация BLE (Nordic UART Service)
  mainArea.addLogLine("Initializing BLE...");

  // Формирование имени устройства: "Имя-Номер"
  snprintf(deviceName, sizeof(deviceName), "%s-%u", device.name.c_str(),
           device.number);
  ESP_LOGI(TAG, "Device name: %s", deviceName);
  char deviceNameStr[40];
  snprintf(deviceNameStr, sizeof(deviceNameStr), "Device: %s", deviceName);
  mainArea.addLogLine(deviceNameStr);

  bleSerial.begin(deviceName);
  ESP_LOGI(TAG, "BLE initialized as: %s", deviceName);

  // Установка callback для мгновенного обновления иконки Bluetooth при
  // изменении состояния
  bleSerial.setStateCallback(onBLEStateChanged);

  // Небольшая задержка для того, чтобы реклама успела запуститься
  delay(50);

  // Обновляем иконку Bluetooth до текущего состояния (реклама уже запущена)
  bleSerial.notifyStateChanged();
  ESP_LOGI(TAG, "BLE ready");
  mainArea.addLogLine("BLE ready");

  // Отрисовка footer с типом модуля
  footer.draw(module_type, VERSION);

  ESP_LOGI(TAG, "Setup complete");
  mainArea.addLogLine("Setup complete");

  // Пауза для чтения лога загрузки (3 секунды)
  delay(5000);

  // Установка типа в зависимости от типа модуля
  if (module_type == ModuleType::START) {
    mainArea.setType(MainAreaType::START);
  } else {
    mainArea.setType(MainAreaType::FINISH);
  }
  mainArea.draw();
}

void loop() {
  gps.update();
  time_sync_update();

  // Обработка BLE данных
  if (bleSerial.available()) {
    String data = bleSerial.readString();
    ESP_LOGD(TAG, "BLE RX: %s", data.c_str());
  }

  // Проверка события и вывод его UTC времени
  int64_t t_esp_us = 0;

  if (event_isr_get(t_esp_us)) {
    int64_t t_utc_us = 0;
    if (time_sync_esp_to_utc_us(t_esp_us, t_utc_us)) {
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
        ESP_LOGI(TAG, "START EVENT LOCAL = %02d:%02d:%02d,%03d", hour, minute,
                 second, local_msec);
      } else {
        ESP_LOGI(TAG, "FINISH EVENT LOCAL = %02d:%02d:%02d,%03d", hour, minute,
                 second, local_msec);
      }
    } else {
      // GPS и RTC ещё не готовы
      if (module_type == ModuleType::START) {
        ESP_LOGW(TAG, "START EVENT esp = %lld us (no time source)",
                 (long long)t_esp_us);
      } else {
        ESP_LOGW(TAG, "FINISH EVENT esp = %lld us (no time source)",
                 (long long)t_esp_us);
      }
    }
  }

  // Обновление статус-бара
  updateStatusBar();
}
