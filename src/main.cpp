#include "config.h"
#include "command/command_parser.h"
#include "esp_log.h"
#include "hal/comm/battery_service.h"
#include "hal/comm/ble.h"
#include "hal/comm/device_info_service.h"
#include "hal/comm/wifi.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "hal/tft/tft.h"
#include "storage/settings.h"
#include "timing/event_isr.h"
#include "timing/event_timestamp.h"
#include "timing/pps_isr.h"
#include "timing/sntp.h"
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

// Парсеры команд для Serial и BLE (инициализируются в setup())
static CommandParser* serialParser = nullptr;
static CommandParser* bleParser = nullptr;

/**
 * @brief Callback для обновления иконки Bluetooth при изменении состояния BLE
 * @param state Текущее состояние Bluetooth
 */
void onBLEStateChanged(BLEState state) { statusBar.updateBluetoothIcon(state); }

/**
 * @brief Callback для обновления иконки WiFi при изменении состояния WiFi
 * @param state Текущее состояние WiFi
 * @param rssi Уровень сигнала в dBm
 */
void onWiFiStateChanged(WiFiState state, int8_t rssi) {
  statusBar.updateWiFiIcon(state, rssi);
}

/**
 * @brief Обновление статус-бара и связанных элементов (когда меняется секунда в
 * системном времени)
 */
void updateStatusBar() {
  static time_t lastTimeSec = 0;

  // Предпочитаем системное время (синхронизируется по GPS PPS)
  time_t nowSec = time(nullptr);

  if (nowSec <= 0)
    return; // Время ещё не установлено

  // Обновляем только когда секунда изменилась в системном времени
  if (nowSec != lastTimeSec) {
    lastTimeSec = nowSec;

    // Получаем таймзону из настроек
    int8_t timezone = settings.getDevice().timezone;

    // Время в статус-баре
    statusBar.updateTime(nowSec, timezone);

    // Статус под статус-баром: спутники + источник времени
    uint8_t sats = gps.isReady() ? gps.nmea().getNumSatellites() : 0;
    TimeSyncStatus ts = time_sync_status();
    const char *src = "NOSYNC";
    if (ts.source == TimeSource::GPS_PPS)
      src = "GPS   ";
    else if (ts.source == TimeSource::RTC)
      src = "RTC   ";

    display.tft().setCursor(0, UI_STATUS_BAR_HEIGHT + 8);
    display.tft().setTextSize(2);
    display.tft().setTextColor(TFT_CYAN, TFT_BLACK);
    display.tft().printf("Sats:%02u  Sync:%s", sats, src);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);

  ESP_LOGI(TAG, "tick rate: %d Hz", configTICK_RATE_HZ);

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
  String moduleTypeStr =
      "Module type: " +
      String(module_type == ModuleType::START ? "START" : "FINISH");
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
    mainArea.addLogLine("RTC initialized");
    // Проверка потери питания
    if (rtc.lostPower()) {
      mainArea.addLogLine("WARN: RTC lost power");
      delay(2000);
    }
  } else {
    mainArea.addLogLine("ERROR: RTC init failed");
    delay(5000);
  }

  // PPS синхронизация от GPS
  pps_init(GPS_PPS_PIN);

  // Инициализация подсистемы синхронизации времени
  mainArea.addLogLine("Starting time sync...");
  time_sync_begin();
  ESP_LOGI(TAG, "Time sync initialized");
  mainArea.addLogLine("Time sync initialized");

  // Инициализация BLE (Nordic UART Service)
  mainArea.addLogLine("Initializing BLE...");

  // Формирование имени устройства: "Имя-Номер"
  String deviceName = device.name + "-" + String(device.number);
  String deviceNameStr = "Device: " + deviceName;
  mainArea.addLogLine(deviceNameStr);

  bleSerial.init(deviceName);
  ESP_LOGI(TAG, "BLE initialized as: %s", deviceName.c_str());

  // Инициализация сервиса батареи
  mainArea.addLogLine("Initializing battery service...");
  bleSerial.registerService(batteryService);
  mainArea.addLogLine("Battery service initialized");
  ESP_LOGI(TAG, "Battery service initialized");

  // Инициализация сервиса устройства
  mainArea.addLogLine("Initializing device info service...");
  bleSerial.registerService(deviceInfoService);
  mainArea.addLogLine("Device info service initialized");
  ESP_LOGI(TAG, "Device info service initialized");

  // Запуск рекламы
  bleSerial.startAdvertising();

  // Установка callback для мгновенного обновления иконки Bluetooth при
  // изменении состояния
  bleSerial.setStateCallback(onBLEStateChanged);

  // Небольшая задержка для того, чтобы реклама успела запуститься
  // delay(50);

  ESP_LOGI(TAG, "BLE ready");
  mainArea.addLogLine("BLE ready");

  // Инициализация парсеров команд для Serial и BLE
  serialParser = new CommandParser(Serial, "Serial");
  bleParser = new CommandParser(bleSerial, "BLE");
  ESP_LOGI(TAG, "Command parsers initialized");

  // Инициализация WiFi (если включен в настройках)
  const WifiSettings &wifi = settings.getWifi();
  if (wifi.active && wifi.ssid.length() > 0) {
    mainArea.addLogLine("Initializing WiFi...");
    wifiManager.begin();
    wifiManager.setStateCallback(onWiFiStateChanged);

    // Подключение к WiFi сети
    if (wifiManager.connect(wifi.ssid, wifi.passwd)) {
      ESP_LOGI(TAG, "WiFi connecting to: %s", wifi.ssid.c_str());
      mainArea.addLogLine("WiFi connecting...");
    } else {
      ESP_LOGE(TAG, "WiFi connect failed");
      mainArea.addLogLine("ERROR: WiFi connect failed");
    }
  } else {
    ESP_LOGI(TAG, "WiFi disabled in settings");
    // Обновляем иконку WiFi до состояния OFF
    statusBar.updateWiFiIcon(WiFiState::OFF, 0);
  }

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
  // Отрисовка footer с типом модуля
  footer.draw(module_type, String(VERSION));
}

void loop() {
  // Обновление статус-бара
  updateStatusBar();

  // Обновление GPS
  gps.update();

  // Обновление синхронизации времени
  time_sync_update();

  // Обновление WiFi (обработка событий, обновление RSSI)
  wifiManager.update();

  // Обработка команд из Serial и BLE
  if (serialParser) {
    serialParser->update();
  }
  if (bleParser) {
    bleParser->update();
  }

  // Проверка события и обработка временного штампа
  int64_t t_esp_us = 0;

  if (event_isr_get(t_esp_us)) {
    EventTimestampData data = event_timestamp_process(t_esp_us, module_type);
    event_timestamp_send_ble(data); // заглушка
    mainArea.displayEventTimestamp(data);
  }
}
