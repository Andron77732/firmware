#include "main_area.h"
#include "storage/settings.h"
#include "esp_log.h"

// Глобальный объект mainArea
MainArea mainArea;
static const char* TAG = "MainArea";

void MainArea::begin(TFT_eSPI& tft) {
    _tft = &tft;
    _canvas = _tft;
    _canvasYOffset = UI_MAIN_AREA_Y_POS;
    _logLineCount = 0;
    _finishLineCount = 0;
    _finishEventCounter = 0;
    _hasEvent = false;

    if (!_framebuffer) {
        _framebuffer = new TFT_eSprite(_tft);
        if (!_framebuffer->createSprite(UI_MAIN_AREA_WIDTH, UI_MAIN_AREA_HEIGHT)) {
            ESP_LOGW(TAG, "Framebuffer disabled: not enough RAM for %dx%d sprite",
                     UI_MAIN_AREA_WIDTH, UI_MAIN_AREA_HEIGHT);
            delete _framebuffer;
            _framebuffer = nullptr;
            _framebufferDisabled = true;
        }
    }
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

    TFT_eSPI* prevCanvas = _canvas;
    uint16_t prevYOffset = _canvasYOffset;

    bool useFramebuffer = !_framebufferDisabled && _framebuffer && _framebuffer->created();
    if (useFramebuffer) {
        _canvas = _framebuffer;
        _canvasYOffset = 0;
    } else {
        _canvas = _tft;
        _canvasYOffset = UI_MAIN_AREA_Y_POS;
    }

    // Очистка области mainArea
    _canvas->fillRect(0, canvasY(0), UI_MAIN_AREA_WIDTH, UI_MAIN_AREA_HEIGHT, UI_MAIN_AREA_COLOR_BACKGROUND);

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

    if (useFramebuffer) {
        _framebuffer->pushSprite(0, UI_MAIN_AREA_Y_POS);
    }

    _canvas = prevCanvas;
    _canvasYOffset = prevYOffset;
}

void MainArea::drawLoading() {
    if (!_canvas) return;

    drawLogLines(_logLines,
                 _logLineCount,
                 UI_MAIN_AREA_LOG_Y,
                 UI_MAIN_AREA_MAX_LOG_LINES,
                 false);
}

void MainArea::displayEventTimestamp(const EventTimestampData& data) {
    if (!data.success)
      return;
    if (_currentType == MainAreaType::FINISH && _hasEvent) {
        if (_lastEvent.success) {
            bool spacerAbove = false;
            const int64_t gap_us = data.utc_timestamp_us - _lastEvent.utc_timestamp_us;
            spacerAbove = gap_us > (int64_t)UI_MAIN_AREA_FINISH_GAP_SPACER_MS * 1000LL;
            addFinishLine(String(_lastEvent.local_time_str), spacerAbove);
        }
    }
    _lastEvent = data;
    _hasEvent = true;
    draw(); // Перерисовываем экран
}

void MainArea::drawStart() {
    if (!_canvas) return;
    
    if (!_hasEvent) {
        // Нет события - пустой экран
        return;
    }
    
    // Отображение локального времени события
    // Используем крупный шрифт для лучшей читаемости
    _canvas->setTextSize(3);
    
    // Определяем цвет в зависимости от успешности
    uint16_t text_color = _lastEvent.success ? TFT_GREEN : TFT_YELLOW;
    _canvas->setTextColor(text_color, UI_MAIN_AREA_COLOR_BACKGROUND);
    
    // Центрируем текст по горизонтали
    // Ширина символа для размера 3: примерно 18px
    // "HH:MM:SS,mmm" = 12 символов = ~216px
    // Центр: (240 - 216) / 2 = 12px от левого края
    uint16_t text_x = 12;
    
    // Позиция по вертикали: примерно в верхней трети экрана
    uint16_t text_y = canvasY(60);
    
    _canvas->setCursor(text_x, text_y);
    _canvas->print(_lastEvent.local_time_str);
}

void MainArea::drawFinish() {
    if (!_canvas) return;
    
    if (!_hasEvent) {
        // Нет события - пустой экран
        return;
    }

    // Отображение текущей отсечки времени вверху
    _canvas->setTextSize(UI_MAIN_AREA_FINISH_TIME_TEXT_SIZE);

    uint16_t text_color = _lastEvent.success ? TFT_GREEN : TFT_YELLOW;
    _canvas->setTextColor(text_color, UI_MAIN_AREA_COLOR_BACKGROUND);

    _canvas->setCursor(UI_MAIN_AREA_FINISH_TIME_X, canvasY(UI_MAIN_AREA_FINISH_TIME_Y));
    _canvas->print(_lastEvent.local_time_str);

    // Предыдущие отсечки под текущей
    drawFinishLines();
}

void MainArea::drawFinishLines() {
    if (!_canvas || _finishLineCount == 0) return;

    _canvas->setTextSize(UI_MAIN_AREA_LOG_TEXT_SIZE);
    _canvas->setTextColor(UI_MAIN_AREA_LOG_COLOR, UI_MAIN_AREA_COLOR_BACKGROUND);

    uint8_t visibleLines = _finishLineCount;
    if (visibleLines > UI_MAIN_AREA_FINISH_MAX_LOG_LINES) {
        visibleLines = UI_MAIN_AREA_FINISH_MAX_LOG_LINES;
    }

    for (uint8_t i = 0; i < visibleLines; i++) {
        uint16_t y = canvasY(UI_MAIN_AREA_FINISH_LOG_Y + i * UI_MAIN_AREA_LOG_LINE_HEIGHT);
        uint32_t number = _finishLineNumbers[i];
        if (number == 0 && _finishLines[i].length() == 0) {
            continue;
        }

        _canvas->setCursor(UI_MAIN_AREA_LOG_X, y);
        if (number < 10) {
            _canvas->print("  ");
        } else if (number < 100) {
            _canvas->print(" ");
        }
        _canvas->print(number);
        _canvas->print(".   ");
        _canvas->print(_finishLines[i]);
    }
}

void MainArea::drawLogLines(const String* lines,
                            uint8_t lineCount,
                            uint16_t startY,
                            uint8_t maxVisibleLines,
                            bool newestAtTop) {
    if (!_canvas || lineCount == 0) return;

    _canvas->setTextSize(UI_MAIN_AREA_LOG_TEXT_SIZE);
    _canvas->setTextColor(UI_MAIN_AREA_LOG_COLOR, UI_MAIN_AREA_COLOR_BACKGROUND);

    uint8_t startLine = 0;
    uint8_t endLine = lineCount;

    if (!newestAtTop && lineCount > maxVisibleLines) {
        startLine = lineCount - maxVisibleLines;
    } else if (newestAtTop && lineCount > maxVisibleLines) {
        endLine = maxVisibleLines;
    }

    for (uint8_t i = startLine; i < endLine; i++) {
        uint8_t displayIndex = newestAtTop ? i : (i - startLine);
        uint16_t y = canvasY(startY + displayIndex * UI_MAIN_AREA_LOG_LINE_HEIGHT);

        uint16_t color = UI_MAIN_AREA_LOG_COLOR;
        String upperLine = lines[i];
        upperLine.toUpperCase();
        if (upperLine.indexOf("ERROR") >= 0 || upperLine.indexOf("FAILED") >= 0) {
            color = UI_MAIN_AREA_LOG_COLOR_ERROR;
        } else if (upperLine.indexOf("WARN") >= 0) {
            color = UI_MAIN_AREA_LOG_COLOR_WARNING;
        }

        _canvas->setTextColor(color, UI_MAIN_AREA_COLOR_BACKGROUND);
        _canvas->setCursor(UI_MAIN_AREA_LOG_X, y);
        _canvas->print(lines[i]);
    }
}

void MainArea::addFinishLine(const String& line, bool spacerAbove) {
    if (line.length() == 0) return;

    uint8_t maxLines = UI_MAIN_AREA_FINISH_MAX_LOG_LINES;
    if (maxLines == 0) return;

    if (spacerAbove && maxLines < 2) {
        spacerAbove = false;
    }

    _finishEventCounter++;
    uint8_t shift = spacerAbove ? 2 : 1;
    uint8_t newCount = _finishLineCount + shift;
    if (newCount > maxLines) {
        newCount = maxLines;
    }

    for (int i = newCount - 1; i >= shift; i--) {
        _finishLines[i] = _finishLines[i - shift];
        _finishLineNumbers[i] = _finishLineNumbers[i - shift];
    }

    _finishLineCount = newCount;

    if (spacerAbove) {
        _finishLines[0] = "";
        _finishLineNumbers[0] = 0;
    }

    if (line.length() > UI_MAIN_AREA_LOG_LINE_LENGTH - 1) {
        _finishLines[spacerAbove ? 1 : 0] =
            line.substring(0, UI_MAIN_AREA_LOG_LINE_LENGTH - 1);
    } else {
        _finishLines[spacerAbove ? 1 : 0] = line;
    }
    _finishLineNumbers[spacerAbove ? 1 : 0] = _finishEventCounter;
}
