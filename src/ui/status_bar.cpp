#include "status_bar.h"
#include "icons.h"

// Глобальный объект статус-бара
StatusBar statusBar;

void StatusBar::begin(TFT_eSPI& tft) {
    _tft = &tft;
    
    // Сброс кэша
    _lastHour = 255;
    _lastMinute = 255;
    _lastSecond = 255;
    _lastBtState = BLEState::DISCONNECTED;
}

void StatusBar::draw() {
    if (!_tft) return;
    
    // Фон статус-бара
    _tft->fillRect(0, Y_POS, WIDTH, HEIGHT, COLOR_BACKGROUND);
    
    // Иконки
    drawIcons();
    
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

void StatusBar::drawIcons() {
    // Пока все иконки неактивны (серые)
    drawIconGPS(COLOR_ICON_INACTIVE);
    drawIconBluetooth(ICON_BT_OFF, COLOR_ICON_INACTIVE);
    drawIconWiFi(COLOR_ICON_INACTIVE);
    drawIconBattery(COLOR_ICON_INACTIVE);
}

void StatusBar::updateBluetoothIcon(BLEState state) {
    if (!_tft) return;
    
    // Проверяем, изменилось ли состояние
    if (state == _lastBtState) {
        return;  // Ничего не изменилось
    }
    
    // Сохраняем текущее значение
    _lastBtState = state;
    
    // Выбираем иконку и цвет в зависимости от состояния
    const uint8_t* bitmap;
    uint16_t color;
    
    switch (state) {
        case BLEState::CONNECTED:
            // Подключено - используем иконку с подключением, синий цвет
            bitmap = ICON_BT_CONNECTED;
            color = COLOR_ICON_BLUETOOTH_ACTIVE;
            break;
            
        case BLEState::ADVERTISING:
            // Реклама активна, но нет подключения - обычная иконка, синий цвет
            bitmap = ICON_BT;
            color = COLOR_ICON_BLUETOOTH_ACTIVE;
            break;
            
        case BLEState::DISCONNECTED:
        default:
            // Реклама не активна - иконка выключена, серый цвет
            bitmap = ICON_BT_OFF;
            color = COLOR_ICON_INACTIVE;
            break;
    }
    
    // Отрисовка иконки с фоном за один проход
    drawIconBluetooth(bitmap, color, COLOR_BACKGROUND);
}

void StatusBar::drawBitmap16(uint16_t x, uint16_t y, const uint8_t* bitmap, uint16_t color, uint16_t bgColor) {
    for (int row = 0; row < 16; row++) {
        uint8_t b1 = bitmap[row * 2];
        uint8_t b2 = bitmap[row * 2 + 1];
        
        for (int col = 0; col < 8; col++) {
            if (b1 & (0x80 >> col)) {
                _tft->drawPixel(x + col, y + row, color);
            } else {
                _tft->drawPixel(x + col, y + row, bgColor);
            }
            if (b2 & (0x80 >> col)) {
                _tft->drawPixel(x + 8 + col, y + row, color);
            } else {
                _tft->drawPixel(x + 8 + col, y + row, bgColor);
            }
        }
    }
}

void StatusBar::drawIconGPS(uint16_t color) {
    drawBitmap16(ICON_GPS_X, ICON_Y, ICON_GPS, color);
}

void StatusBar::drawIconBluetooth(const uint8_t* bitmap, uint16_t color, uint16_t bgColor) {
    drawBitmap16(ICON_BLUETOOTH_X, ICON_Y, bitmap, color, bgColor);
}

void StatusBar::drawIconWiFi(uint16_t color) {
    drawBitmap16(ICON_WIFI_X, ICON_Y, ICON_WIFI_0, color);
}

void StatusBar::drawIconBattery(uint16_t color) {
    drawBitmap16(ICON_BATTERY_X, ICON_Y, ICON_BAT_FULL, color);
}
