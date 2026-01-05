#ifndef UI_MAIN_AREA_H
#define UI_MAIN_AREA_H

#include <Arduino.h>
#include <TFT_eSPI.h>

/**
 * @brief Тип отображения mainArea
 */
enum class MainAreaType {
    LOADING,  // Загрузка модуля
    START,    // Старт
    FINISH    // Финиш
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
    // Размеры и позиции
    static constexpr uint16_t WIDTH = 240;
    static constexpr uint16_t HEIGHT = 284;  // 320 - 24 - 12
    static constexpr uint16_t Y_POS = 24;    // После status_bar
    
    // Цвета
    static constexpr uint16_t COLOR_BACKGROUND = TFT_BLACK;

    /**
     * @brief Инициализация mainArea
     * @param tft Ссылка на TFT объект
     */
    void begin(TFT_eSPI& tft);
    
    /**
     * @brief Установка типа отображения
     * @param type Тип отображения (LOADING/START/FINISH)
     */
    void setType(MainAreaType type);
    
    /**
     * @brief Полная отрисовка mainArea
     */
    void draw();
    
    /**
     * @brief Добавить строку в лог загрузки
     * @param line Текст строки (максимум 39 символов)
     */
    void addLogLine(const char* line);
    
    /**
     * @brief Очистить лог загрузки
     */
    void clearLog();

private:
    TFT_eSPI* _tft = nullptr;
    MainAreaType _currentType = MainAreaType::LOADING;
    
    // Параметры отображения логов
    static constexpr uint16_t LOG_X = 4;
    static constexpr uint16_t LOG_Y = 4;
    static constexpr uint8_t LOG_TEXT_SIZE = 1;  // 8px height per line
    static constexpr uint8_t LOG_LINE_SPACING = 4;  // Дополнительный отступ между строками (px)
    static constexpr uint8_t LOG_PADDING_RIGHT = 4;  // Отступ справа для безопасности
    
    // Параметры шрифта (для размера 1 в TFT_eSPI: 6px ширина, 8px высота)
    static constexpr uint8_t LOG_CHAR_HEIGHT = 8;  // Ширина одного символа в пикселях для размера 1
    static constexpr uint8_t LOG_CHAR_WIDTH = 6;  // Ширина одного символа в пикселях для размера 1
    
    // Буфер для логов загрузки (вычисляется динамически)
    static constexpr uint16_t LOG_TEXT_HEIGHT = LOG_CHAR_HEIGHT * LOG_TEXT_SIZE;  // Высота текста одной строки
    static constexpr uint16_t LOG_LINE_HEIGHT = LOG_TEXT_HEIGHT + LOG_LINE_SPACING;  // Полная высота строки с отступом
    static constexpr uint16_t LOG_AVAILABLE_HEIGHT = HEIGHT - LOG_Y;  // Доступная высота для логов
    static constexpr uint8_t MAX_LOG_LINES = LOG_AVAILABLE_HEIGHT / LOG_LINE_HEIGHT;  // Максимальное количество строк
    static constexpr uint16_t LOG_AVAILABLE_WIDTH = WIDTH - LOG_X - LOG_PADDING_RIGHT;  // Доступная ширина для логов
    static constexpr uint8_t LOG_LINE_LENGTH = (LOG_AVAILABLE_WIDTH / LOG_CHAR_WIDTH) + 1;  // Максимальная длина строки (+1 для '\0')
    char _logLines[MAX_LOG_LINES][LOG_LINE_LENGTH];
    uint8_t _logLineCount = 0;
    static constexpr uint16_t LOG_COLOR = TFT_WHITE;
    static constexpr uint16_t LOG_COLOR_ERROR = TFT_RED;
    static constexpr uint16_t LOG_COLOR_WARNING = TFT_YELLOW;
    
    /**
     * @brief Отрисовка режима загрузки (логи)
     */
    void drawLoading();
    
    /**
     * @brief Отрисовка режима старт
     */
    void drawStart();
    
    /**
     * @brief Отрисовка режима финиш
     */
    void drawFinish();
};

// Глобальный объект mainArea
extern MainArea mainArea;

#endif // UI_MAIN_AREA_H

