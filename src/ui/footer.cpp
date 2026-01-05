#include "footer.h"

// Глобальный объект footer
Footer footer;

void Footer::begin(TFT_eSPI& tft) {
    _tft = &tft;
}

void Footer::draw(ModuleType moduleType, const char* version) {
    if (!_tft) return;
    
    // Отрисовка фона footer
    _tft->fillRect(0, Y_POS, WIDTH, HEIGHT, COLOR_BACKGROUND);
    
    // Подготовка текста
    const char* moduleTypeStr = (moduleType == ModuleType::START) ? "START" : "FINISH";
    
    // Настройка текста
    _tft->setTextSize(TEXT_SIZE);
    _tft->setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _tft->setCursor(TEXT_X, Y_POS + TEXT_Y);
    
    // Вывод текста: "START v0.1.0" или "FINISH v0.1.0"
    _tft->printf("%s v%s", moduleTypeStr, version);
}

