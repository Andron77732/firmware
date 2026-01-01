#include <Arduino.h>
#include "config.h"
#include "hal/tft/tft.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    // Инициализация дисплея
    display.begin();
    
    // Тестовое сообщение
    display.tft().setCursor(10, 10);
    display.tft().println("ENTime v0.1");
    display.tft().println("TFT OK");
}

void loop() {
    // TODO: основной цикл
}
