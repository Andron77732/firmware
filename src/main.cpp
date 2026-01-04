#include "config.h"
#include "esp_log.h"
#include "hal/comm/ble.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "hal/tft/tft.h"
#include "timing/event_isr.h"
#include "timing/pps_isr.h"
#include "timing/time_sync.h"
#include "ui/status_bar.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <time.h>

static const char *TAG = "MAIN";

/**
 * @brief Callback для обновления иконки Bluetooth при изменении состояния BLE
 * @param state Текущее состояние Bluetooth
 */
void onBLEStateChanged(BLEState state) { statusBar.updateBluetoothIcon(state); }

/**
 * @brief Обновление статус-бара и связанных элементов (каждую секунду)
 */
void updateStatusBar()
{
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();

  // Обновляем каждую секунду
  if (now - lastUpdate >= 1000)
  {
    lastUpdate = now;

    // Предпочитаем системное время (синхронизируется по GPS PPS)
    time_t nowSec = time(nullptr);

    uint8_t hour = 0, minute = 0, second = 0;
    uint16_t year = 1970;
    uint8_t month = 1, day = 1;

    if (nowSec > 0)
    {
      struct tm tm{};
      gmtime_r(&nowSec, &tm);
      year = static_cast<uint16_t>(tm.tm_year + 1900);
      month = static_cast<uint8_t>(tm.tm_mon + 1);
      day = static_cast<uint8_t>(tm.tm_mday);
      hour = static_cast<uint8_t>(tm.tm_hour);
      minute = static_cast<uint8_t>(tm.tm_min);
      second = static_cast<uint8_t>(tm.tm_sec);
    }

    // Время в статус-баре
    statusBar.updateTime(hour, minute, second);
    // ESP_LOGD(TAG, "Time: %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);

    // Обновление иконки Bluetooth (fallback проверка)
    // statusBar.updateBluetoothIcon(bleSerial.getState());

    // Статус под статус-баром: спутники + источник времени
    uint8_t sats = gps.isReady() ? gps.nmea().getNumSatellites() : 0;
    TimeSyncStatus ts = time_sync_status();
    const char *src = "NOSYNC";
    if (ts.source == TimeSource::GPS_PPS)
      src = "GPS";
    else if (ts.source == TimeSource::RTC)
      src = "RTC";

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
      ESP_LOGI(TAG, "EVENT UTC = %lld us", (long long)t_utc_us);
    }
    else
    {
      // GPS и RTC ещё не готовы
      ESP_LOGW(TAG, "EVENT esp = %lld us (no time source)", (long long)t_esp_us);
    }
  }

  // Обновление статус-бара
  updateStatusBar();
}
