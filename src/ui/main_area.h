#ifndef UI_MAIN_AREA_H
#define UI_MAIN_AREA_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "hal/touch/touch.h"
#include "ui_touch_target.h"
#include "ui_config.h"
#include "timing/event_timestamp.h"

/**
 * @brief Тип отображения mainArea
 */
enum class MainAreaType {
    LOADING,  // Загрузка модуля
    START,    // Старт
    FINISH    // Финиш
};

enum class StartCountdownMode {
    COUNTDOWN,
    GO,
    HIDDEN
};

/**
 * @brief Основная область экрана между status_bar и footer
 * 
 * Layout (240px width, 284px height):
 * ┌────────────────────────────────────────┐
 * │                                        │
 * │         Main Area Content              │
 * │                                        │
 * └────────────────────────────────────────┘
 * 
 * Занимает всю оставшуюся область после status_bar (24px) и footer (12px)
 */
class MainArea {
public:

    /**
     * @brief Инициализация mainArea
     * @param tft Ссылка на TFT объект
     */
    void init(TFT_eSPI& tft);
    
    /**
     * @brief Установка типа отображения
     * @param type Тип отображения (LOADING/START/FINISH)
     */
    void setType(MainAreaType type);

    /**
     * @brief Получить текущий тип отображения
     */
    MainAreaType getType() const { return _currentType; }
    
    /**
     * @brief Полная отрисовка mainArea
     */
    void draw();
    
    /**
     * @brief Добавить строку в лог загрузки
     * @param line Текст строки
     */
    void addLogLine(const String& line);
    
    /**
     * @brief Очистить лог загрузки
     */
    void clearLog();
    
    /**
     * @brief Отобразить временной штамп события на экране
     * @param data Данные временного штампа события
     */
    void displayEventTimestamp(const EventTimestampData& data);

    /**
     * @brief Обновить обратный отсчет для режима START
     * @param seconds Секунды текущей минуты
     */
    void updateCountdown(uint8_t seconds);

    /**
     * @brief Обработчик touch-событий
     * @return true если событие обработано
     */
    bool onTouchEvent(const TouchEvent& event, UiTouchTarget& target);

private:
    TFT_eSPI* _tft = nullptr;
    TFT_eSPI* _canvas = nullptr;
    TFT_eSprite* _framebuffer = nullptr;
    bool _framebufferDisabled = false;
    uint16_t _canvasYOffset = UI_MAIN_AREA_Y_POS;
    MainAreaType _currentType = MainAreaType::LOADING;
    
    // Буфер для логов загрузки
    String _logLines[UI_MAIN_AREA_MAX_LOG_LINES];
    uint8_t _logLineCount = 0;

    // Буфер для стартовых событий (сверху вниз, новые сверху)
    String _startLines[UI_MAIN_AREA_MAX_LOG_LINES];
    uint32_t _startLineNumbers[UI_MAIN_AREA_MAX_LOG_LINES];
    uint8_t _startLineCount = 0;
    uint32_t _startEventCounter = 0;

    // Буфер для отсечек финиша (сверху вниз)
    String _finishLines[UI_MAIN_AREA_FINISH_MAX_LOG_LINES];
    uint32_t _finishLineNumbers[UI_MAIN_AREA_FINISH_MAX_LOG_LINES];
    uint8_t _finishLineCount = 0;
    uint32_t _finishEventCounter = 0;
    
    // Данные последнего события
    EventTimestampData _lastEvent;
    bool _hasEvent = false;
    UiTouchTarget _touchCapturedTarget = UiTouchTarget::None;
    StartCountdownMode _countdownMode = StartCountdownMode::HIDDEN;
    uint8_t _countdownValue = 0;
    
    /**
     * @brief Отрисовка режима загрузки (логи)
     */
    bool containsPoint_(const TouchPoint& point) const;
    void drawLoading();
    
    /**
     * @brief Отрисовка режима старт
     */
    void drawStart();
    
    /**
     * @brief Отрисовка режима финиш
     */
    void drawFinish();

    /**
     * @brief Отрисовка списка отсечек финиша
     */
    void drawFinishLines();

    /**
     * @brief Преобразовать координату Y в координаты canvas
     */
    uint16_t canvasY(uint16_t localY) const { return localY + _canvasYOffset; }

    /**
     * @brief Отрисовка списка строк с форматированием как у лога
     */
    void drawLogLines(const String* lines,
                      uint8_t lineCount,
                      uint16_t startY,
                      uint8_t maxVisibleLines,
                      bool newestAtTop);

    /**
     * @brief Добавить строку в список отсечек финиша
     */
    void addFinishLine(const String& line, bool spacerAbove);

    /**
     * @brief Добавить строку в список стартовых событий
     */
    void addStartLine(const String& line);

    /**
     * @brief Отрисовка списка стартовых событий
     */
    void drawStartLines(uint16_t startY, uint8_t maxVisibleLines);
};

#endif // UI_MAIN_AREA_H
