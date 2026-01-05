#ifndef UI_MAIN_AREA_H
#define UI_MAIN_AREA_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "ui_config.h"

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
    
    // Буфер для логов загрузки
    char _logLines[UI_MAIN_AREA_MAX_LOG_LINES][UI_MAIN_AREA_LOG_LINE_LENGTH];
    uint8_t _logLineCount = 0;
    
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

