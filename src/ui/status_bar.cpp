#include "status_bar.h"

// Глобальный объект статус-бара
StatusBar statusBar;

void StatusBar::begin(TFT_eSPI& tft) {
    _tft = &tft;
    
    // Сброс кэша
    _lastHour = 255;
    _lastMinute = 255;
    _lastSecond = 255;
}

void StatusBar::draw() {
    if (!_tft) return;
    
    // Фон статус-бара
    _tft->fillRect(0, Y_POS, WIDTH, HEIGHT, COLOR_BACKGROUND);
    
    // Placeholder'ы иконок
    drawIconPlaceholders();
    
    // Принудительная отрисовка времени
    forceRedrawTime();
}

void StatusBar::updateTime(uint8_t hour, uint8_t minute, uint8_t second) {
    if (!_tft) return;
    
    // Проверяем, изменилось ли время
    if (hour == _lastHour && minute == _lastMinute && second == _lastSecond) {
        return;  // Ничего не изменилось
    }
    
    // Сохраняем текущие значения
    _lastHour = hour;
    _lastMinute = minute;
    _lastSecond = second;
    
    // Отрисовка времени
    _tft->setTextSize(CLOCK_TEXT_SIZE);
    _tft->setTextColor(COLOR_CLOCK, COLOR_BACKGROUND);
    _tft->setCursor(CLOCK_X, CLOCK_Y);
    _tft->printf("%02d:%02d:%02d", hour, minute, second);
}

void StatusBar::forceRedrawTime() {
    // Сбрасываем кэш, чтобы updateTime точно перерисовал
    uint8_t h = _lastHour;
    uint8_t m = _lastMinute;
    uint8_t s = _lastSecond;
    
    _lastHour = 255;
    _lastMinute = 255;
    _lastSecond = 255;
    
    // Если были валидные значения, перерисовываем их
    if (h != 255) {
        updateTime(h, m, s);
    } else {
        // Иначе рисуем placeholder
        _tft->setTextSize(CLOCK_TEXT_SIZE);
        _tft->setTextColor(COLOR_CLOCK, COLOR_BACKGROUND);
        _tft->setCursor(CLOCK_X, CLOCK_Y);
        _tft->print("--:--:--");
    }
}

void StatusBar::drawIconPlaceholders() {
    // Рисуем 4 placeholder'а для будущих иконок (слева направо)
    drawIconPlaceholder(ICON_GPS_X,       ICON_Y, ICON_SIZE, COLOR_ICON_PLACEHOLDER);
    drawIconPlaceholder(ICON_BLUETOOTH_X, ICON_Y, ICON_SIZE, COLOR_ICON_PLACEHOLDER);
    drawIconPlaceholder(ICON_WIFI_X,      ICON_Y, ICON_SIZE, COLOR_ICON_PLACEHOLDER);
    drawIconPlaceholder(ICON_BATTERY_X,   ICON_Y, ICON_SIZE, COLOR_ICON_PLACEHOLDER);
}

void StatusBar::drawIconPlaceholder(uint16_t x, uint16_t y, uint16_t size, uint16_t color) {
    // Рисуем пустой прямоугольник как placeholder
    _tft->drawRect(x, y, size, size, color);
}

