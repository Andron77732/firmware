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
    _lastSats = 255;
    _lastState = (TimeSyncState)255;
    
    // Отрисовка фона footer
    _tft->fillRect(0, UI_FOOTER_Y_POS, UI_FOOTER_WIDTH, UI_FOOTER_HEIGHT, UI_FOOTER_COLOR_BACKGROUND);
    
    // Подготовка текста
    const char* moduleTypeStr = (moduleType == ModuleType::START) ? "START" : "FINISH";
    
    // Настройка текста
    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(UI_FOOTER_TEXT_X, UI_FOOTER_Y_POS + UI_FOOTER_TEXT_Y);
    
    // Вывод текста: "START v0.1.0" или "FINISH v0.1.0"
    _tft->printf("%s v%s", moduleTypeStr, version.c_str());
}

void Footer::updateTimeSyncState(uint8_t sats, TimeSyncState state) {
    // Обновляем только если что-то изменилось
    if (!_tft || (state == _lastState && sats == _lastSats)) {
        return;
    }
    
    _lastState = state;
    _lastSats = sats;
    
   
    const char* label = timeSyncLabel(state);
    uint16_t color    = timeSyncColor(state);

    // Формируем короткую строку справа
    // Sxx + пробел + LABEL
    char right[16];
    snprintf(right, sizeof(right), "S%02u %s", (unsigned)sats, label);

    int16_t x = UI_FOOTER_WIDTH - 6 * (int)strlen(right) * UI_FOOTER_TEXT_SIZE;
    int16_t y = UI_FOOTER_Y_POS + UI_FOOTER_TEXT_Y;

    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);

    // спутники — нейтральным цветом
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(x, y);
    _tft->printf("S%02u ", (unsigned)sats);

    // статус — цветной
    int16_t x2 = x + 6 * 4 * UI_FOOTER_TEXT_SIZE; // "Sxx " = 4 символа
    _tft->setTextColor(color, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(x2, y);
    _tft->print(label);
}
