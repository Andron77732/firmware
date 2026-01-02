#include <Arduino.h>
#include "esp_log.h"
#include "config.h"
#include "hal/tft/tft.h"
#include "hal/gps/gps.h"

static const char* TAG = "MAIN";

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);
    
    // Инициализация дисплея
    display.begin();
    
    // Заголовок
    display.tft().setCursor(0, 0);
    display.tft().setTextSize(2);
    display.tft().setTextColor(TFT_WHITE, TFT_BLACK);
    display.tft().printf("ENTime v%s", VERSION);
    
    display.tft().setCursor(0, 25);
    display.tft().setTextSize(1);
    display.tft().setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.tft().print("GPS Time Sync");
    
    // Инициализация GPS
    gps.begin();
    
    ESP_LOGI(TAG, "Setup complete");
}

void loop() {
    gps.update();
}
