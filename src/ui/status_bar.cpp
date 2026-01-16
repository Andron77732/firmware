#include "status_bar.h"
#include "icons.h"
#include "timing/time_sync.h"
#include <time.h>

// Глобальный объект статус-бара
StatusBar statusBar;

static uint16_t clockColorForState() {
    return (time_sync_state() == TimeSyncState::NONE)
               ? UI_STATUS_BAR_COLOR_CLOCK_NO_SYNC
               : UI_STATUS_BAR_COLOR_CLOCK;
}

void StatusBar::begin(TFT_eSPI& tft) {
    _tft = &tft;
    
    // Сброс кэша
    _lastBtState = BLEState::DISCONNECTED;
    _lastWiFiState = WiFiState::UNINITIALIZED;
    _lastWiFiSignalLevel = 255;
}

void StatusBar::draw() {
    if (!_tft) return;
    
    // Фон статус-бара
    _tft->fillRect(0, UI_STATUS_BAR_Y_POS, UI_STATUS_BAR_WIDTH, UI_STATUS_BAR_HEIGHT, UI_STATUS_BAR_COLOR_BACKGROUND);
    
    // Иконки
    drawIcons();
    
    // Принудительная отрисовка времени
    drawTimePlaceholder();
}

void StatusBar::updateTime(const struct tm &local_tm) {
    if (!_tft) return;
    
    // Отрисовка времени
    _tft->setTextSize(UI_STATUS_BAR_CLOCK_TEXT_SIZE);
    _tft->setTextColor(clockColorForState(), UI_STATUS_BAR_COLOR_BACKGROUND);
    _tft->setCursor(UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y);
    _tft->printf("%02d:%02d:%02d", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
}

void StatusBar::drawTimePlaceholder() {
    // Рисуем placeholder, если время еще не было установлено
    _tft->setTextSize(UI_STATUS_BAR_CLOCK_TEXT_SIZE);
    _tft->setTextColor(clockColorForState(), UI_STATUS_BAR_COLOR_BACKGROUND);
    _tft->setCursor(UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y);
    _tft->print("--:--:--");
}

void StatusBar::drawIcons() {
    // Пока все иконки неактивны (серые)
    drawIconGPS(UI_STATUS_BAR_COLOR_ICON_INACTIVE, UI_STATUS_BAR_COLOR_BACKGROUND);
    drawIconBluetooth(ICON_BT_OFF, UI_STATUS_BAR_COLOR_ICON_INACTIVE, UI_STATUS_BAR_COLOR_BACKGROUND);
    drawIconWiFi(ICON_WIFI_OFF, UI_STATUS_BAR_COLOR_ICON_INACTIVE, UI_STATUS_BAR_COLOR_BACKGROUND);
    drawIconBattery(UI_STATUS_BAR_COLOR_ICON_INACTIVE, UI_STATUS_BAR_COLOR_BACKGROUND);
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

void StatusBar::updateWiFiIcon(WiFiState state, int8_t rssi) {
    if (!_tft) return;
    
    // Определяем уровень сигнала (0-4) на основе RSSI
    uint8_t signalLevel = 255; // 255 = не определен
    if (state == WiFiState::CONNECTED) {
        if (rssi > UI_WIFI_RSSI_LEVEL_4) {
            signalLevel = 4;  // Отличный сигнал
        } else if (rssi > UI_WIFI_RSSI_LEVEL_3) {
            signalLevel = 3;  // Хороший сигнал
        } else if (rssi > UI_WIFI_RSSI_LEVEL_2) {
            signalLevel = 2;  // Средний сигнал
        } else if (rssi > UI_WIFI_RSSI_LEVEL_1) {
            signalLevel = 1;  // Слабый сигнал
        } else {
            signalLevel = 0;  // Очень слабый сигнал
        }
    }
    
    // Проверяем, изменилось ли состояние или уровень сигнала
    // (сравниваем уровень сигнала, а не точное значение RSSI)
    if (state == _lastWiFiState && signalLevel == _lastWiFiSignalLevel) {
        return;  // Ничего не изменилось
    }
    
    // Сохраняем текущие значения
    _lastWiFiState = state;
    _lastWiFiSignalLevel = signalLevel;
    
    // Выбираем иконку и цвет в зависимости от состояния
    const uint8_t* bitmap;
    uint16_t color;
    
    switch (state) {
        case WiFiState::UNINITIALIZED:
            // WiFiManager не инициализирован - иконка выключена, серый цвет
            bitmap = ICON_WIFI_OFF;
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
            
        case WiFiState::OFF:
            // WiFi выключен - иконка выключена, серый цвет
            bitmap = ICON_WIFI_OFF;
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
            
        case WiFiState::ERROR:
            // Ошибка подключения - иконка с предупреждением, желтый/красный цвет
            bitmap = ICON_WIFI_ALERT;
            color = TFT_YELLOW;
            break;
            
        case WiFiState::CONNECTING:
            // Идет подключение - минимальная иконка, желтый цвет
            bitmap = ICON_WIFI_0;
            color = TFT_YELLOW;
            break;
            
        case WiFiState::CONNECTED:
            // Подключено - выбираем иконку по уровню сигнала, белый/зеленый цвет
            switch (signalLevel) {
                case 4:
                    bitmap = ICON_WIFI_4;  // Отличный сигнал
                    break;
                case 3:
                    bitmap = ICON_WIFI_3;  // Хороший сигнал
                    break;
                case 2:
                    bitmap = ICON_WIFI_2;  // Средний сигнал
                    break;
                case 1:
                    bitmap = ICON_WIFI_1;  // Слабый сигнал
                    break;
                case 0:
                default:
                    bitmap = ICON_WIFI_0;  // Очень слабый сигнал
                    break;
            }
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_ACTIVE;
            break;
            
        case WiFiState::DISCONNECTED:
        default:
            // Не подключено (но WiFi включен) - минимальная иконка, серый цвет
            bitmap = ICON_WIFI_0;
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
    }
    
    // Отрисовка иконки с фоном за один проход
    drawIconWiFi(bitmap, color, UI_STATUS_BAR_COLOR_BACKGROUND);
}

void StatusBar::updateGPSIcon(GPSState state) {
    if (!_tft) return;

    if (state == _lastGpsState) {
        return;
    }

    _lastGpsState = state;

    uint16_t color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
    switch (state) {
        case GPSState::ACTIVE:
            color = UI_STATUS_BAR_COLOR_ICON_GPS_ACTIVE;
            break;
        case GPSState::SEARCHING:
            color = UI_STATUS_BAR_COLOR_ICON_GPS_SEARCHING;
            break;
        case GPSState::OFF:
        default:
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
    }

    drawIconGPS(color, UI_STATUS_BAR_COLOR_BACKGROUND);
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

void StatusBar::drawIconGPS(uint16_t color, uint16_t bgColor) {
    drawBitmap16(UI_STATUS_BAR_ICON_GPS_X, UI_STATUS_BAR_ICON_Y, ICON_GPS, color, bgColor);
}

void StatusBar::drawIconBluetooth(const uint8_t* bitmap, uint16_t color, uint16_t bgColor) {
    drawBitmap16(UI_STATUS_BAR_ICON_BLUETOOTH_X, UI_STATUS_BAR_ICON_Y, bitmap, color, bgColor);
}

void StatusBar::drawIconWiFi(const uint8_t* bitmap, uint16_t color, uint16_t bgColor) {
    drawBitmap16(UI_STATUS_BAR_ICON_WIFI_X, UI_STATUS_BAR_ICON_Y, bitmap, color, bgColor);
}

void StatusBar::drawIconBattery(uint16_t color, uint16_t bgColor) {
    drawBitmap16(UI_STATUS_BAR_ICON_BATTERY_X, UI_STATUS_BAR_ICON_Y, ICON_BAT_FULL, color, bgColor);
}
