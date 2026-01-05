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
    _tft->fillRect(0, UI_STATUS_BAR_Y_POS, UI_STATUS_BAR_WIDTH, UI_STATUS_BAR_HEIGHT, UI_STATUS_BAR_COLOR_BACKGROUND);
    
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
    _tft->setTextSize(UI_STATUS_BAR_CLOCK_TEXT_SIZE);
    _tft->setTextColor(UI_STATUS_BAR_COLOR_CLOCK, UI_STATUS_BAR_COLOR_BACKGROUND);
    _tft->setCursor(UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y);
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
        _tft->setTextSize(UI_STATUS_BAR_CLOCK_TEXT_SIZE);
        _tft->setTextColor(UI_STATUS_BAR_COLOR_CLOCK, UI_STATUS_BAR_COLOR_BACKGROUND);
        _tft->setCursor(UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y);
        _tft->print("--:--:--");
    }
}

void StatusBar::drawIcons() {
    // Пока все иконки неактивны (серые)
    drawIconGPS(UI_STATUS_BAR_COLOR_ICON_INACTIVE);
    drawIconBluetooth(ICON_BT_OFF, UI_STATUS_BAR_COLOR_ICON_INACTIVE);
    drawIconWiFi(UI_STATUS_BAR_COLOR_ICON_INACTIVE);
    drawIconBattery(UI_STATUS_BAR_COLOR_ICON_INACTIVE);
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
            color = UI_STATUS_BAR_COLOR_ICON_BLUETOOTH_ACTIVE;
            break;
            
        case BLEState::ADVERTISING:
            // Реклама активна, но нет подключения - обычная иконка, синий цвет
            bitmap = ICON_BT;
            color = UI_STATUS_BAR_COLOR_ICON_BLUETOOTH_ACTIVE;
            break;
            
        case BLEState::DISCONNECTED:
        default:
            // Реклама не активна - иконка выключена, серый цвет
            bitmap = ICON_BT_OFF;
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
    }
    
    // Отрисовка иконки с фоном за один проход
    drawIconBluetooth(bitmap, color, UI_STATUS_BAR_COLOR_BACKGROUND);
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
    drawBitmap16(UI_STATUS_BAR_ICON_GPS_X, UI_STATUS_BAR_ICON_Y, ICON_GPS, color);
}

void StatusBar::drawIconBluetooth(const uint8_t* bitmap, uint16_t color, uint16_t bgColor) {
    drawBitmap16(UI_STATUS_BAR_ICON_BLUETOOTH_X, UI_STATUS_BAR_ICON_Y, bitmap, color, bgColor);
}

void StatusBar::drawIconWiFi(uint16_t color) {
    drawBitmap16(UI_STATUS_BAR_ICON_WIFI_X, UI_STATUS_BAR_ICON_Y, ICON_WIFI_0, color);
}

void StatusBar::drawIconBattery(uint16_t color) {
    drawBitmap16(UI_STATUS_BAR_ICON_BATTERY_X, UI_STATUS_BAR_ICON_Y, ICON_BAT_FULL, color);
}
