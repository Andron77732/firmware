#include "status_bar.h"
#include "icons.h"
#include "timing/time_sync.h"
#include <time.h>

static uint16_t clockColorForState() {
    return (time_sync_state() == TimeSyncState::NONE)
               ? UI_STATUS_BAR_COLOR_CLOCK_NO_SYNC
               : UI_STATUS_BAR_COLOR_CLOCK;
}

void StatusBar::init(TFT_eSPI& tft) {
    _tft = &tft;
    _btState = BLEState::DISCONNECTED;
    _wifiState = WiFiState::UNINITIALIZED;
    _wifiSignalLevel = 255;
    _gpsState = GPSState::OFF;
    _batteryLevel = InaBatteryLevel::NoData;
    _batteryCharging = false;
    _batteryChargeFrame = 0;
    _hasTime = false;
    _touchCapturedTarget = UiTouchTarget::None;
    _time = {};
}

void StatusBar::draw() {
    if (!_tft) return;
    
    // Фон статус-бара
    _tft->fillRect(0, UI_STATUS_BAR_Y_POS, UI_STATUS_BAR_WIDTH, UI_STATUS_BAR_HEIGHT, UI_STATUS_BAR_COLOR_BACKGROUND);
    
    drawTimeValue();
    drawBluetoothValue(_btState);
    drawWiFiValue(_wifiState, _wifiSignalLevel);
    drawGpsValue(_gpsState);
    drawBatteryValue(_batteryLevel);
}

void StatusBar::updateTime(const struct tm &local_tm) {
    if (!_tft) return;
    _time = local_tm;
    _hasTime = true;
    drawTimeValue();
}

void StatusBar::drawTimePlaceholder() {
    // Рисуем placeholder, если время еще не было установлено
    if (!_tft) return;

    _tft->setTextSize(UI_STATUS_BAR_CLOCK_TEXT_SIZE);
    _tft->setTextColor(clockColorForState(), UI_STATUS_BAR_COLOR_BACKGROUND);
    _tft->setCursor(UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y);
    _tft->print("--:--:--");
}

uint8_t StatusBar::wifiSignalLevelFromRssi(WiFiState state, int8_t rssi) const {
    if (state != WiFiState::CONNECTED) {
        return 255;
    }

    if (rssi > UI_WIFI_RSSI_LEVEL_4) {
        return 4;
    }
    if (rssi > UI_WIFI_RSSI_LEVEL_3) {
        return 3;
    }
    if (rssi > UI_WIFI_RSSI_LEVEL_2) {
        return 2;
    }
    if (rssi > UI_WIFI_RSSI_LEVEL_1) {
        return 1;
    }
    return 0;
}

void StatusBar::drawTimeValue() {
    if (!_tft) return;

    if (!_hasTime) {
        drawTimePlaceholder();
        return;
    }

    _tft->setTextSize(UI_STATUS_BAR_CLOCK_TEXT_SIZE);
    _tft->setTextColor(clockColorForState(), UI_STATUS_BAR_COLOR_BACKGROUND);
    _tft->setCursor(UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y);
    _tft->printf("%02d:%02d:%02d", _time.tm_hour, _time.tm_min, _time.tm_sec);
}

void StatusBar::drawBluetoothValue(BLEState state) {
    const uint8_t* bitmap = ICON_BT_OFF;
    uint16_t color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;

    switch (state) {
        case BLEState::CONNECTED:
            bitmap = ICON_BT_CONNECTED;
            color = UI_STATUS_BAR_COLOR_ICON_BLUETOOTH_ACTIVE;
            break;
        case BLEState::ADVERTISING:
            bitmap = ICON_BT;
            color = UI_STATUS_BAR_COLOR_ICON_BLUETOOTH_ACTIVE;
            break;
        case BLEState::DISCONNECTED:
        default:
            bitmap = ICON_BT_OFF;
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
    }

    drawIconBluetooth(bitmap, color, UI_STATUS_BAR_COLOR_BACKGROUND);
}

void StatusBar::drawWiFiValue(WiFiState state, uint8_t signalLevel) {
    const uint8_t* bitmap = ICON_WIFI_OFF;
    uint16_t color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;

    switch (state) {
        case WiFiState::UNINITIALIZED:
            bitmap = ICON_WIFI_OFF;
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_UNINITIALIZED;
            break;
        case WiFiState::OFF:
            bitmap = ICON_WIFI_OFF;
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_OFF;
            break;
        case WiFiState::ERROR:
            bitmap = ICON_WIFI_ALERT;
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_ERROR;
            break;
        case WiFiState::CONNECTING:
            bitmap = ICON_WIFI_0;
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_CONNECTING;
            break;
        case WiFiState::RECONNECTING:
            bitmap = ICON_WIFI_0;
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_RECONNECTING;
            break;
        case WiFiState::CONNECTED:
            switch (signalLevel) {
                case 4:
                    bitmap = ICON_WIFI_4;
                    break;
                case 3:
                    bitmap = ICON_WIFI_3;
                    break;
                case 2:
                    bitmap = ICON_WIFI_2;
                    break;
                case 1:
                    bitmap = ICON_WIFI_1;
                    break;
                case 0:
                default:
                    bitmap = ICON_WIFI_0;
                    break;
            }
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_CONNECTED;
            break;
        case WiFiState::DISCONNECTED:
        default:
            bitmap = ICON_WIFI_0;
            color = UI_STATUS_BAR_COLOR_ICON_WIFI_DISCONNECTED;
            break;
    }

    drawIconWiFi(bitmap, color, UI_STATUS_BAR_COLOR_BACKGROUND);
}

void StatusBar::drawGpsValue(GPSState state) {
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

void StatusBar::drawBatteryValue(InaBatteryLevel level) {
    if (_batteryCharging) {
        const uint8_t* bitmap = ICON_BAT_LOW;
        switch (_batteryChargeFrame % 3) {
            case 0:
                bitmap = ICON_BAT_LOW;
                break;
            case 1:
                bitmap = ICON_BAT_MID;
                break;
            case 2:
            default:
                bitmap = ICON_BAT_FULL;
                break;
        }
        drawIconBattery(bitmap, UI_STATUS_BAR_COLOR_ICON_BATTERY_CHARGING,
                        UI_STATUS_BAR_COLOR_BACKGROUND);
        return;
    }

    const uint8_t* bitmap = ICON_BAT_EMPTY;
    uint16_t color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;

    switch (level) {
        case InaBatteryLevel::NoData:
            bitmap = ICON_BAT_EMPTY;
            color = UI_STATUS_BAR_COLOR_ICON_INACTIVE;
            break;
        case InaBatteryLevel::Critical:
            bitmap = ICON_BAT_EMPTY;
            color = TFT_RED;
            break;
        case InaBatteryLevel::Empty:
            bitmap = ICON_BAT_EMPTY;
            color = UI_STATUS_BAR_COLOR_ICON_ACTIVE;
            break;
        case InaBatteryLevel::Low:
            bitmap = ICON_BAT_LOW;
            color = UI_STATUS_BAR_COLOR_ICON_ACTIVE;
            break;
        case InaBatteryLevel::Mid:
            bitmap = ICON_BAT_MID;
            color = UI_STATUS_BAR_COLOR_ICON_ACTIVE;
            break;
        case InaBatteryLevel::Full:
        default:
            bitmap = ICON_BAT_FULL;
            color = UI_STATUS_BAR_COLOR_ICON_ACTIVE;
            break;
    }

    drawIconBattery(bitmap, color, UI_STATUS_BAR_COLOR_BACKGROUND);
}

void StatusBar::updateBluetoothIcon(BLEState state) {
    if (!_tft) return;

    if (state == _btState) {
        return;
    }

    _btState = state;
    drawBluetoothValue(_btState);
}

void StatusBar::updateWiFiIcon(WiFiState state, int8_t rssi) {
    if (!_tft) return;

    const uint8_t signalLevel = wifiSignalLevelFromRssi(state, rssi);
    if (state == _wifiState && signalLevel == _wifiSignalLevel) {
        return;
    }

    _wifiState = state;
    _wifiSignalLevel = signalLevel;
    drawWiFiValue(_wifiState, _wifiSignalLevel);
}

void StatusBar::updateGPSIcon(GPSState state) {
    if (!_tft) return;

    if (state == _gpsState) {
        return;
    }

    _gpsState = state;
    drawGpsValue(_gpsState);
}

void StatusBar::updateBatteryIcon(InaBatteryLevel level, bool charging) {
    if (!_tft) return;

    if (!charging && level == _batteryLevel && !_batteryCharging) {
        return;
    }

    if (!charging) {
        _batteryLevel = level;
        _batteryCharging = false;
        _batteryChargeFrame = 0;
        drawBatteryValue(_batteryLevel);
        return;
    }

    if (!_batteryCharging) {
        _batteryLevel = level;
        _batteryCharging = true;
        _batteryChargeFrame = 0;
        drawBatteryValue(_batteryLevel);
        return;
    }

    _batteryLevel = level;
    _batteryChargeFrame = (_batteryChargeFrame + 1) % 3;
    drawBatteryValue(_batteryLevel);
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

void StatusBar::drawIconBattery(const uint8_t* bitmap, uint16_t color, uint16_t bgColor) {
    drawBitmap16(UI_STATUS_BAR_ICON_BATTERY_X, UI_STATUS_BAR_ICON_Y, bitmap, color, bgColor);
}

bool StatusBar::containsPoint_(const TouchPoint& point) const {
    return point.x < UI_STATUS_BAR_WIDTH &&
           point.y >= UI_STATUS_BAR_Y_POS &&
           point.y < (UI_STATUS_BAR_Y_POS + UI_STATUS_BAR_HEIGHT);
}

bool StatusBar::containsRect_(const TouchPoint& point,
                              uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height) const {
    return point.x >= x && point.x < (x + width) &&
           point.y >= y && point.y < (y + height);
}

UiTouchTarget StatusBar::targetAtPoint_(const TouchPoint& point) const {
    if (!containsPoint_(point)) {
        return UiTouchTarget::None;
    }

    if (containsRect_(point, UI_STATUS_BAR_CLOCK_X, UI_STATUS_BAR_CLOCK_Y,
                      UI_STATUS_BAR_CLOCK_WIDTH,
                      UI_STATUS_BAR_CLOCK_HEIGHT)) {
        return UiTouchTarget::StatusBarClock;
    }

    if (containsRect_(point, UI_STATUS_BAR_ICON_GPS_X, UI_STATUS_BAR_ICON_Y,
                      UI_STATUS_BAR_ICON_SIZE, UI_STATUS_BAR_ICON_SIZE)) {
        return UiTouchTarget::StatusBarGps;
    }

    if (containsRect_(point, UI_STATUS_BAR_ICON_BLUETOOTH_X,
                      UI_STATUS_BAR_ICON_Y, UI_STATUS_BAR_ICON_SIZE,
                      UI_STATUS_BAR_ICON_SIZE)) {
        return UiTouchTarget::StatusBarBluetooth;
    }

    if (containsRect_(point, UI_STATUS_BAR_ICON_WIFI_X, UI_STATUS_BAR_ICON_Y,
                      UI_STATUS_BAR_ICON_SIZE, UI_STATUS_BAR_ICON_SIZE)) {
        return UiTouchTarget::StatusBarWifi;
    }

    if (containsRect_(point, UI_STATUS_BAR_ICON_BATTERY_X,
                      UI_STATUS_BAR_ICON_Y, UI_STATUS_BAR_ICON_SIZE,
                      UI_STATUS_BAR_ICON_SIZE)) {
        return UiTouchTarget::StatusBarBattery;
    }

    return UiTouchTarget::StatusBarBackground;
}

bool StatusBar::onTouchEvent(const TouchEvent& event, UiTouchTarget& target) {
    switch (event.type) {
        case TouchEventType::Press:
            _touchCapturedTarget = targetAtPoint_(event.point);
            target = _touchCapturedTarget;
            return target != UiTouchTarget::None;

        case TouchEventType::Release: {
            target = _touchCapturedTarget;
            const bool handled = target != UiTouchTarget::None;
            _touchCapturedTarget = UiTouchTarget::None;
            return handled;
        }

        case TouchEventType::Move:
        default:
            target = UiTouchTarget::None;
            return false;
    }
}
