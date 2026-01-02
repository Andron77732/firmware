#include <Arduino.h>
#include "esp_log.h"
#include "config.h"
#include "hal/tft/tft.h"
#include "hal/gps/gps.h"
#include "hal/rtc/rtc.h"
#include "ui/status_bar.h"

static const char* TAG = "MAIN";

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);
    
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
    
    ESP_LOGI(TAG, "Setup complete");
}

void loop() {
    gps.update();
    
    // Обновление статус-бара каждую секунду
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    if (now - lastUpdate >= 1000) {
        lastUpdate = now;
        DateTime dt = rtc.now();
        
        // Время в статус-баре
        statusBar.updateTime(dt.hour(), dt.minute(), dt.second());
        
        // Дата под статус-баром
        display.tft().setCursor(0, StatusBar::HEIGHT + 60);
        display.tft().setTextSize(2);
        display.tft().setTextColor(TFT_CYAN, TFT_BLACK);
        display.tft().printf("%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
    }
}
