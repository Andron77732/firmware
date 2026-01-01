#include <Arduino.h>
#include "esp_log.h"
#include "config.h"
#include "hal/tft/tft.h"

static const char* TAG = "MAIN";

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    ESP_LOGI(TAG, "ENTime v%s starting...", VERSION);
    
    // Инициализация дисплея
    display.begin();
    ESP_LOGI(TAG, "Display initialized");
    
    // Тестовое сообщение на экране
    display.tft().setCursor(0, 0);
      display.tft().printf("ENTime v%s\n", VERSION);
      display.tft().println("TFT OK");
    
    ESP_LOGI(TAG, "Setup complete");
}

void loop() {
    // TODO: основной цикл
}
