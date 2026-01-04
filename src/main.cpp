#include "config.h"
#include "esp_log.h"
#include "hal/comm/ble.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "hal/tft/tft.h"
#include "timing/event_isr.h"
#include "ui/status_bar.h"
#include <Arduino.h>

static const char *TAG = "MAIN";

/**
 * @brief Callback для обновления иконки Bluetooth при изменении состояния BLE
 * @param state Текущее состояние Bluetooth
 */
void onBLEStateChanged(BLEState state) { statusBar.updateBluetoothIcon(state); }

/**
 * @brief Обновление статус-бара и связанных элементов (каждую секунду)
 */
void updateStatusBar() {
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();

  // Обновляем каждую секунду
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;
    DateTime dt = rtc.now();

    // Время в статус-баре
    statusBar.updateTime(dt.hour(), dt.minute(), dt.second());

    // Обновление иконки Bluetooth (fallback проверка)
    // statusBar.updateBluetoothIcon(bleSerial.getState());

    // Дата под статус-баром
    display.tft().setCursor(0, StatusBar::HEIGHT + 60);
    display.tft().setTextSize(2);
    display.tft().setTextColor(TFT_CYAN, TFT_BLACK);
    display.tft().printf("%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);

  // Инициализация прерывания на событие
  event_isr_init(EXT_INT_PIN);
  ESP_LOGI(TAG, "Event ISR initialized");

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

  // Инициализация GPS
  gps.begin();

  // Инициализация RTC
  rtc.begin();

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

void loop() {
  gps.update();

  // Обработка BLE данных
  if (bleSerial.available()) {
    String data = bleSerial.readString();
    ESP_LOGD(TAG, "BLE RX: %s", data.c_str());
  }

  int64_t t = 0;
  // Обработка события прерывания
  if (event_isr_get(t)) {
    ESP_LOGD(TAG, "EVENT at %lld us\n", t);
  }

  // Обновление статус-бара
  updateStatusBar();
}
