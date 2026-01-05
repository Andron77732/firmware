#include "main_area.h"

// Глобальный объект mainArea
MainArea mainArea;

void MainArea::begin(TFT_eSPI& tft) {
    _tft = &tft;
    _logLineCount = 0;
}

void MainArea::setType(MainAreaType type) {
    _currentType = type;
}

void MainArea::addLogLine(const String& line) {
    if (line.length() == 0) return;
    
    // Если буфер заполнен, сдвигаем строки вверх
    if (_logLineCount >= UI_MAIN_AREA_MAX_LOG_LINES) {
        for (uint8_t i = 0; i < UI_MAIN_AREA_MAX_LOG_LINES - 1; i++) {
            _logLines[i] = _logLines[i + 1];
        }
        _logLineCount = UI_MAIN_AREA_MAX_LOG_LINES - 1;
    }
    
    // Добавляем новую строку (обрезаем если слишком длинная)
    if (line.length() > UI_MAIN_AREA_LOG_LINE_LENGTH - 1) {
        _logLines[_logLineCount] = line.substring(0, UI_MAIN_AREA_LOG_LINE_LENGTH - 1);
    } else {
        _logLines[_logLineCount] = line;
    }
    _logLineCount++;
    
    // Если тип LOADING, перерисовываем
    if (_currentType == MainAreaType::LOADING) {
        drawLoading();
    }
}

void MainArea::clearLog() {
    _logLineCount = 0;
    if (_currentType == MainAreaType::LOADING) {
        draw();
    }
}

void MainArea::draw() {
    if (!_tft) return;
    
    // Очистка области mainArea
    _tft->fillRect(0, UI_MAIN_AREA_Y_POS, UI_MAIN_AREA_WIDTH, UI_MAIN_AREA_HEIGHT, UI_MAIN_AREA_COLOR_BACKGROUND);
    
    // Отрисовка в зависимости от типа
    switch (_currentType) {
        case MainAreaType::LOADING:
            drawLoading();
            break;
        case MainAreaType::START:
            drawStart();
            break;
        case MainAreaType::FINISH:
            drawFinish();
            break;
    }
}

void MainArea::drawLoading() {
    if (!_tft) return;
    
    // Настройка текста
    _tft->setTextSize(UI_MAIN_AREA_LOG_TEXT_SIZE);
    _tft->setTextColor(UI_MAIN_AREA_LOG_COLOR, UI_MAIN_AREA_COLOR_BACKGROUND);
    
    // Используем предвычисленные константы для определения количества видимых строк
    uint16_t maxVisibleLines = UI_MAIN_AREA_MAX_LOG_LINES;
    
    // Определяем начальную строку для отображения (прокрутка снизу)
    uint8_t startLine = 0;
    if (_logLineCount > maxVisibleLines) {
        startLine = _logLineCount - maxVisibleLines;
    }
    
    // Отрисовка строк лога
    for (uint8_t i = startLine; i < _logLineCount; i++) {
        uint16_t y = UI_MAIN_AREA_Y_POS + UI_MAIN_AREA_LOG_Y + (i - startLine) * UI_MAIN_AREA_LOG_LINE_HEIGHT;
        
        // Определяем цвет в зависимости от содержимого
        uint16_t color = UI_MAIN_AREA_LOG_COLOR;
        String upperLine = _logLines[i];
        upperLine.toUpperCase();
        if (upperLine.indexOf("ERROR") >= 0 || upperLine.indexOf("FAILED") >= 0) {
            color = UI_MAIN_AREA_LOG_COLOR_ERROR;
        } else if (upperLine.indexOf("WARN") >= 0) {
            color = UI_MAIN_AREA_LOG_COLOR_WARNING;
        }
        
        _tft->setTextColor(color, UI_MAIN_AREA_COLOR_BACKGROUND);
        _tft->setCursor(UI_MAIN_AREA_LOG_X, y);
        _tft->print(_logLines[i]);
    }
}

void MainArea::drawStart() {
    if (!_tft) return;
    // TODO: Реализовать отрисовку режима старт
}

void MainArea::drawFinish() {
    if (!_tft) return;
    // TODO: Реализовать отрисовку режима финиш
}

