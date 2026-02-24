#include "footer.h"
#include "TFT_eSPI.h"

// Глобальный объект footer
Footer footer;

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

void Footer::begin(TFT_eSPI& tft) {
    _tft = &tft;
}

void Footer::draw(ModuleType moduleType, const String& version) {
    if (!_tft) return;

    // Сбрасываем последние значения
    _lastSats = -127;
    _lastState = (TimeSyncState)255;
    
    // Отрисовка фона footer
    _tft->fillRect(0, UI_FOOTER_Y_POS, UI_FOOTER_WIDTH, UI_FOOTER_HEIGHT, UI_FOOTER_COLOR_BACKGROUND);
    
    // Подготовка текста
    const char* moduleTypeStr = (moduleType == ModuleType::START) ? "START" : "FINISH";
    
    // Настройка текста
    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(UI_FOOTER_TEXT_X, UI_FOOTER_Y_POS + UI_FOOTER_PADDING);
    
    // Вывод текста: "START v0.1.0" или "FINISH v0.1.0"
    _tft->printf("%s v%s", moduleTypeStr, version.c_str());
}

void Footer::updateSats(int8_t sats) {
    if (!_tft || sats == _lastSats) {
        return;
    }

    _lastSats = sats;

    const char* label = timeSyncLabel(_lastState);
    int16_t x = UI_FOOTER_WIDTH - 6 * (4 + (int)strlen(label)) * UI_FOOTER_TEXT_SIZE;
    int16_t y = UI_FOOTER_Y_POS + UI_FOOTER_PADDING;

    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(x, y);

    if (sats < 0) {
        _tft->print("S-- ");
    } else {
        _tft->printf("S%02d ", (int)sats);
    }
}

void Footer::updateTimeSyncState(TimeSyncState state) {
    // Обновляем только если что-то изменилось
    if (!_tft || state == _lastState) {
        return;
    }
    
    _lastState = state;
    
    const char* label = timeSyncLabel(state);
    uint16_t color    = timeSyncColor(state);

    // статус — цветной
    int16_t x = UI_FOOTER_WIDTH - 6 * (4 + (int)strlen(label)) * UI_FOOTER_TEXT_SIZE;
    int16_t y = UI_FOOTER_Y_POS + UI_FOOTER_PADDING;
    int16_t x2 = x + 6 * 4 * UI_FOOTER_TEXT_SIZE; // "Sxx " = 4 символа
    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(color, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(x2, y);
    _tft->print(label);
}

bool Footer::onTouchEvent(const TouchEvent& event) {
    (void)event;
    return false;
}
