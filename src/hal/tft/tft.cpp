#include "tft.h"

// Глобальный объект дисплея
Display display;

void Display::begin(uint8_t rotation) {
    if (_initialized) return;
    
    _tft.init();
    _tft.setRotation(rotation);
    _tft.fillScreen(TFT_BLACK);
    _tft.setTextColor(TFT_WHITE, TFT_BLACK);
    _tft.setTextSize(2);
    
    _initialized = true;
}

void Display::clear(uint16_t color) {
    _tft.fillScreen(color);
}
