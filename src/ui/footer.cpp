#include "footer.h"
#include "TFT_eSPI.h"

static const char* timeSyncLabel(TimeSyncState s) {
    switch (s) {
        case TimeSyncState::GPS_OK:        return "GPS OK ";
        case TimeSyncState::GPS_DEGRADED:  return "GPS DEG";
        case TimeSyncState::RTC_OK:        return "RTC OK ";
        case TimeSyncState::RTC_DEGRADED:  return "RTC DEG";
        default:                           return "NOSYNC ";
    }
}

static uint16_t timeSyncColor(TimeSyncState s) {
    switch (s) {
        case TimeSyncState::GPS_OK:        return TFT_GREEN;
        case TimeSyncState::GPS_DEGRADED:  return TFT_YELLOW;
        case TimeSyncState::RTC_OK:        return TFT_CYAN;
        case TimeSyncState::RTC_DEGRADED:  return TFT_YELLOW;
        default:                           return TFT_RED;
    }
}

void Footer::init(TFT_eSPI& tft, ModuleType moduleType, const String& version) {
    _tft = &tft;
    _touchCaptured = false;
    _moduleType = moduleType;
    _version = version;
}

void Footer::draw() {
    if (!_tft) return;
    
    // Отрисовка фона footer
    _tft->fillRect(0, UI_FOOTER_Y_POS, UI_FOOTER_WIDTH, UI_FOOTER_HEIGHT, UI_FOOTER_COLOR_BACKGROUND);
    
    // Подготовка текста
    const char* moduleTypeStr = (_moduleType == ModuleType::START) ? "START" : "FINISH";
    
    // Настройка текста
    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(UI_FOOTER_TEXT_X, UI_FOOTER_Y_POS + UI_FOOTER_PADDING);
    
    // Вывод текста: "START v0.1.0" или "FINISH v0.1.0"
    _tft->printf("%s v%s", moduleTypeStr, _version.c_str());

    drawSatsValue(_sats, _state);
    drawTimeSyncValue(_state, _sats);
}

void Footer::drawSatsValue(int8_t sats, TimeSyncState state) {
    if (!_tft) {
        return;
    }

    const char* label = timeSyncLabel(state);
    int16_t x = UI_FOOTER_WIDTH - 6 * (4 + (int)strlen(label)) * UI_FOOTER_TEXT_SIZE;
    int16_t y = UI_FOOTER_Y_POS + UI_FOOTER_PADDING;

    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(x, y);

    if (sats < 0) {
        _tft->print("S-- ");
        return;
    }

    _tft->printf("S%02d ", (int)sats);
}

void Footer::drawTimeSyncValue(TimeSyncState state, int8_t sats) {
    if (!_tft) {
        return;
    }

    const char* label = timeSyncLabel(state);
    uint16_t color = timeSyncColor(state);
    int16_t x = UI_FOOTER_WIDTH - 6 * (4 + (int)strlen(label)) * UI_FOOTER_TEXT_SIZE;
    int16_t y = UI_FOOTER_Y_POS + UI_FOOTER_PADDING;

    // "Sxx " = 4 символа для стандартного формата.
    int16_t satsChars = 4;
    if (sats >= 100) {
        satsChars = 5;
    }
    int16_t x2 = x + 6 * satsChars * UI_FOOTER_TEXT_SIZE;

    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(color, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(x2, y);
    _tft->print(label);
}

void Footer::updateSats(int8_t sats) {
    if (!_tft || sats == _sats) {
        return;
    }

    _sats = sats;
    drawSatsValue(_sats, _state);
    drawTimeSyncValue(_state, _sats);
}

void Footer::updateTimeSyncState(TimeSyncState state) {
    // Обновляем только если что-то изменилось
    if (!_tft || state == _state) {
        return;
    }
    
    _state = state;
    drawSatsValue(_sats, _state);
    drawTimeSyncValue(_state, _sats);
}

bool Footer::containsPoint_(const TouchPoint& point) const {
    return point.x < UI_FOOTER_WIDTH &&
           point.y >= UI_FOOTER_Y_POS &&
           point.y < (UI_FOOTER_Y_POS + UI_FOOTER_HEIGHT);
}

bool Footer::onTouchEvent(const TouchEvent& event) {
    switch (event.type) {
        case TouchEventType::Press:
            _touchCaptured = containsPoint_(event.point);
            return _touchCaptured;

        case TouchEventType::Release: {
            const bool handled = _touchCaptured;
            _touchCaptured = false;
            return handled;
        }

        case TouchEventType::Move:
        default:
            return false;
    }
}
