#include "main_area.h"
#include "storage/settings.h"
#include "esp_log.h"
#include <string.h>
static const char* TAG = "MainArea";

void MainArea::init(TFT_eSPI& tft) {
    _tft = &tft;
    _canvas = _tft;
    _canvasYOffset = UI_MAIN_AREA_Y_POS;
    _logLineCount = 0;
    _loadingScrollLines = 0;
    _loadingDragLastY = 0;
    _loadingDragAccumPx = 0;
    _loadingDragActive = false;
    _startLineCount = 0;
    _startEventCounter = 0;
    _finishLineCount = 0;
    _finishEventCounter = 0;
    _hasEvent = false;
    _touchCapturedTarget = UiTouchTarget::None;
    _countdownMode = StartCountdownMode::HIDDEN;
    _countdownValue = 0;

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

bool MainArea::containsPoint_(const TouchPoint& point) const {
    return point.x < UI_MAIN_AREA_WIDTH &&
           point.y >= UI_MAIN_AREA_Y_POS &&
           point.y < (UI_MAIN_AREA_Y_POS + UI_MAIN_AREA_HEIGHT);
}

void MainArea::setType(MainAreaType type) {
    _currentType = type;
    if (_currentType != MainAreaType::LOADING) {
        _loadingDragActive = false;
        _loadingDragAccumPx = 0;
    }
}

void MainArea::addLogLine(const String& line) {
    if (line.length() == 0) return;
    const bool autoScroll = _loadingScrollLines == 0;
    
    // Если буфер заполнен, сдвигаем строки вверх
    if (_logLineCount >= UI_MAIN_AREA_LOG_HISTORY_LINES) {
        for (uint8_t i = 0; i < UI_MAIN_AREA_LOG_HISTORY_LINES - 1; i++) {
            _logLines[i] = _logLines[i + 1];
        }
        _logLineCount = UI_MAIN_AREA_LOG_HISTORY_LINES - 1;
    }
    
    // Добавляем новую строку (обрезаем если слишком длинная)
    if (line.length() > UI_MAIN_AREA_LOG_LINE_LENGTH - 1) {
        _logLines[_logLineCount] = line.substring(0, UI_MAIN_AREA_LOG_LINE_LENGTH - 1);
    } else {
        _logLines[_logLineCount] = line;
    }
    _logLineCount++;
    if (autoScroll) {
        _loadingScrollLines = 0;
    } else {
        clampLoadingScroll_();
    }
    
    // Если тип LOADING, перерисовываем
    if (_currentType == MainAreaType::LOADING) {
        draw();
    }
}

void MainArea::clearLog() {
    _logLineCount = 0;
    _loadingScrollLines = 0;
    _loadingDragActive = false;
    _loadingDragAccumPx = 0;
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
    clampLoadingScroll_();

    drawLogLines(_logLines,
                 _logLineCount,
                 UI_MAIN_AREA_LOG_Y,
                 UI_MAIN_AREA_MAX_LOG_LINES,
                 false,
                 _loadingScrollLines);
}

void MainArea::displayEventTimestamp(const EventTimestampData& data) {
    if (!data.success)
      return;
    if (data.module_type == ModuleType::FINISH) {
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
        return;
    }

    if (data.module_type == ModuleType::START) {
        int32_t correction_ms = data.correction_ms;
        int32_t abs_corr = (correction_ms < 0) ? -correction_ms : correction_ms;
        char line[UI_MAIN_AREA_LOG_LINE_LENGTH];
        if (abs_corr <= (int32_t)MAX_CORRECTION_MS) {
            snprintf(line, sizeof(line), "%s  %+ld", data.local_time_str, (long)correction_ms);
        } else {
            snprintf(line, sizeof(line), "%s", data.local_time_str);
        }
        addStartLine(String(line));
        _lastEvent = data;
        _hasEvent = true;
        draw(); // Перерисовываем экран
        return;
    }
}

void MainArea::drawStart() {
    if (!_canvas) return;

    char countdown[3] = "";
    bool showCountdown = false;
    if (_countdownMode == StartCountdownMode::GO) {
        snprintf(countdown, sizeof(countdown), "GO");
        showCountdown = true;
    } else if (_countdownMode == StartCountdownMode::COUNTDOWN) {
        snprintf(countdown, sizeof(countdown), "%u", (unsigned)_countdownValue);
        showCountdown = true;
    }

    _canvas->setTextSize(UI_MAIN_AREA_START_COUNTDOWN_TEXT_SIZE);
    _canvas->setTextColor(TFT_WHITE, UI_MAIN_AREA_COLOR_BACKGROUND);

    uint16_t countdown_char_width = UI_CHAR_WIDTH * UI_MAIN_AREA_START_COUNTDOWN_TEXT_SIZE;
    uint16_t countdown_width = countdown_char_width * strlen(countdown);
    uint16_t countdown_x = (UI_MAIN_AREA_WIDTH - countdown_width) / 2;
    uint16_t countdown_y = UI_MAIN_AREA_START_COUNTDOWN_Y;
    if (showCountdown) {
        _canvas->setCursor(countdown_x, canvasY(countdown_y));
        _canvas->print(countdown);
    }

    uint16_t next_y = countdown_y + UI_MAIN_AREA_START_COUNTDOWN_HEIGHT +
                      UI_MAIN_AREA_START_COUNTDOWN_SPACING;

    uint16_t rect_height = UI_MAIN_AREA_START_CORRECTION_HEIGHT +
                           UI_MAIN_AREA_START_CORRECTION_PADDING * 2;
    _canvas->fillRect(0, canvasY(next_y), UI_MAIN_AREA_WIDTH, rect_height,
                      UI_MAIN_AREA_START_CORRECTION_COLOR_BACKGROUND);

    _canvas->setTextSize(UI_MAIN_AREA_START_CORRECTION_TEXT_SIZE);
    _canvas->setTextColor(UI_MAIN_AREA_START_CORRECTION_COLOR_TEXT,
                          UI_MAIN_AREA_START_CORRECTION_COLOR_BACKGROUND);

    char correction_text[12];
    bool has_valid_correction = false;
    int32_t correction_ms = 0;
    if (_hasEvent && _lastEvent.success) {
        correction_ms = _lastEvent.correction_ms;
        int32_t abs_corr = (correction_ms < 0) ? -correction_ms : correction_ms;
        has_valid_correction = abs_corr <= (int32_t)MAX_CORRECTION_MS;
    }

    if (has_valid_correction) {
        snprintf(correction_text, sizeof(correction_text), "%ld",
                 (long)correction_ms);
    }

    uint16_t correction_x = UI_MAIN_AREA_START_CORRECTION_PADDING;
    uint16_t correction_y = next_y + UI_MAIN_AREA_START_CORRECTION_PADDING +
                            UI_MAIN_AREA_START_CORRECTION_TEXT_Y_OFFSET;
    if (has_valid_correction) {
        _canvas->setCursor(correction_x, canvasY(correction_y));
        _canvas->print(correction_text);
    }

    next_y += rect_height + UI_MAIN_AREA_START_CORRECTION_SPACING;

    if (_startLineCount > 0) {
        uint16_t available_height = 0;
        if (next_y < UI_MAIN_AREA_HEIGHT) {
            available_height = UI_MAIN_AREA_HEIGHT - next_y;
        }
        uint8_t max_lines = available_height / UI_MAIN_AREA_LOG_LINE_HEIGHT;
        if (max_lines > 0) {
            drawStartLines(next_y, max_lines);
        }
    }
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
                            bool newestAtTop,
                            uint8_t scrollLines) {
    if (!_canvas || lineCount == 0) return;

    _canvas->setTextSize(UI_MAIN_AREA_LOG_TEXT_SIZE);
    _canvas->setTextColor(UI_MAIN_AREA_LOG_COLOR, UI_MAIN_AREA_COLOR_BACKGROUND);

    uint8_t startLine = 0;
    uint8_t endLine = lineCount;

    if (!newestAtTop && lineCount > maxVisibleLines) {
        uint8_t maxScroll = lineCount - maxVisibleLines;
        if (scrollLines > maxScroll) {
            scrollLines = maxScroll;
        }
        startLine = lineCount - maxVisibleLines - scrollLines;
        endLine = startLine + maxVisibleLines;
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

void MainArea::updateCountdown(uint8_t seconds) {
    StartCountdownMode nextMode = StartCountdownMode::HIDDEN;
    uint8_t nextValue = 0;
    if (seconds <= 10) {
        nextMode = StartCountdownMode::GO;
    } else if (seconds >= 30) {
        nextMode = StartCountdownMode::COUNTDOWN;
        nextValue = (uint8_t)(60 - seconds);
    }

    if (nextMode == _countdownMode && nextValue == _countdownValue) {
        return;
    }
    _countdownMode = nextMode;
    _countdownValue = nextValue;
    if (_currentType == MainAreaType::START) {
        draw();
    }
}

void MainArea::addStartLine(const String& line) {
    if (line.length() == 0) return;

    uint8_t maxLines = UI_MAIN_AREA_MAX_LOG_LINES;
    if (maxLines == 0) return;

    _startEventCounter++;
    uint8_t newCount = _startLineCount + 1;
    if (newCount > maxLines) {
        newCount = maxLines;
    }

    for (int i = newCount - 1; i >= 1; i--) {
        _startLines[i] = _startLines[i - 1];
        _startLineNumbers[i] = _startLineNumbers[i - 1];
    }

    if (line.length() > UI_MAIN_AREA_LOG_LINE_LENGTH - 1) {
        _startLines[0] = line.substring(0, UI_MAIN_AREA_LOG_LINE_LENGTH - 1);
    } else {
        _startLines[0] = line;
    }
    _startLineNumbers[0] = _startEventCounter;

    _startLineCount = newCount;
}

void MainArea::drawStartLines(uint16_t startY, uint8_t maxVisibleLines) {
    if (!_canvas || _startLineCount == 0 || maxVisibleLines == 0) return;

    _canvas->setTextSize(UI_MAIN_AREA_LOG_TEXT_SIZE);
    _canvas->setTextColor(UI_MAIN_AREA_LOG_COLOR, UI_MAIN_AREA_COLOR_BACKGROUND);

    uint8_t visibleLines = _startLineCount;
    if (visibleLines > maxVisibleLines) {
        visibleLines = maxVisibleLines;
    }

    for (uint8_t i = 0; i < visibleLines; i++) {
        uint16_t y = canvasY(startY + i * UI_MAIN_AREA_LOG_LINE_HEIGHT);
        uint32_t number = _startLineNumbers[i];
        if (number == 0 && _startLines[i].length() == 0) {
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
        _canvas->print(_startLines[i]);
    }
}

void MainArea::clampLoadingScroll_() {
    uint8_t maxScroll = 0;
    if (_logLineCount > UI_MAIN_AREA_MAX_LOG_LINES) {
        maxScroll = _logLineCount - UI_MAIN_AREA_MAX_LOG_LINES;
    }
    if (_loadingScrollLines > maxScroll) {
        _loadingScrollLines = maxScroll;
    }
}

bool MainArea::handleLoadingTouch_(const TouchEvent& event) {
    switch (event.type) {
        case TouchEventType::Press:
            _loadingDragActive = true;
            _loadingDragLastY = (int16_t)event.point.y;
            _loadingDragAccumPx = 0;
            return true;

        case TouchEventType::Move: {
            if (!_loadingDragActive) {
                return false;
            }

            int16_t y = (int16_t)event.point.y;
            int16_t dy = y - _loadingDragLastY;
            _loadingDragLastY = y;
            _loadingDragAccumPx += dy;

            bool changed = false;
            while (_loadingDragAccumPx >= (int16_t)UI_MAIN_AREA_LOG_LINE_HEIGHT) {
                uint8_t maxScroll = 0;
                if (_logLineCount > UI_MAIN_AREA_MAX_LOG_LINES) {
                    maxScroll = _logLineCount - UI_MAIN_AREA_MAX_LOG_LINES;
                }
                if (_loadingScrollLines < maxScroll) {
                    _loadingScrollLines++;
                    changed = true;
                }
                _loadingDragAccumPx -= UI_MAIN_AREA_LOG_LINE_HEIGHT;
            }
            while (_loadingDragAccumPx <= -(int16_t)UI_MAIN_AREA_LOG_LINE_HEIGHT) {
                if (_loadingScrollLines > 0) {
                    _loadingScrollLines--;
                    changed = true;
                }
                _loadingDragAccumPx += UI_MAIN_AREA_LOG_LINE_HEIGHT;
            }

            if (changed) {
                draw();
            }
            return true;
        }

        case TouchEventType::Release:
            _loadingDragActive = false;
            _loadingDragAccumPx = 0;
            return true;

        default:
            return false;
    }
}

bool MainArea::onTouchEvent(const TouchEvent& event, UiTouchTarget& target) {
    switch (event.type) {
        case TouchEventType::Press:
            _touchCapturedTarget =
                containsPoint_(event.point) ? UiTouchTarget::MainArea
                                            : UiTouchTarget::None;
            target = _touchCapturedTarget;
            if (target != UiTouchTarget::None && _currentType == MainAreaType::LOADING) {
                handleLoadingTouch_(event);
            }
            return target != UiTouchTarget::None;

        case TouchEventType::Move:
            target = _touchCapturedTarget;
            if (target != UiTouchTarget::None &&
                _currentType == MainAreaType::LOADING) {
                return handleLoadingTouch_(event);
            }
            target = UiTouchTarget::None;
            return false;

        case TouchEventType::Release: {
            target = _touchCapturedTarget;
            const bool handled = target != UiTouchTarget::None;
            if (handled && _currentType == MainAreaType::LOADING) {
                handleLoadingTouch_(event);
            }
            _touchCapturedTarget = UiTouchTarget::None;
            return handled;
        }

        default:
            target = UiTouchTarget::None;
            return false;
    }
}
