#include "footer.h"

// Глобальный объект footer
Footer footer;

void Footer::begin(TFT_eSPI& tft) {
    _tft = &tft;
}

void Footer::draw(ModuleType moduleType, const char* version) {
    if (!_tft) return;
    
    // Отрисовка фона footer
    _tft->fillRect(0, UI_FOOTER_Y_POS, UI_FOOTER_WIDTH, UI_FOOTER_HEIGHT, UI_FOOTER_COLOR_BACKGROUND);
    
    // Подготовка текста
    const char* moduleTypeStr = (moduleType == ModuleType::START) ? "START" : "FINISH";
    
    // Настройка текста
    _tft->setTextSize(UI_FOOTER_TEXT_SIZE);
    _tft->setTextColor(UI_FOOTER_COLOR_TEXT, UI_FOOTER_COLOR_BACKGROUND);
    _tft->setCursor(UI_FOOTER_TEXT_X, UI_FOOTER_Y_POS + UI_FOOTER_TEXT_Y);
    
    // Вывод текста: "START v0.1.0" или "FINISH v0.1.0"
    _tft->printf("%s v%s", moduleTypeStr, version);
}

