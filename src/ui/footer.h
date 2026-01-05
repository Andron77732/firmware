#ifndef UI_FOOTER_H
#define UI_FOOTER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

/**
 * @brief Footer в нижней части экрана
 * 
 * Layout (240px width, 12px height):
 * ┌────────────────────────────────────────┐
 * │ START v0.1.0                           │
 * └────────────────────────────────────────┘
 * 
 * Отображает тип модуля и версию прошивки слева
 */
class Footer {
public:
    // Размеры и позиции
    static constexpr uint16_t WIDTH = 240;
    static constexpr uint16_t HEIGHT = 12;
    static constexpr uint16_t Y_POS = 308;  // 320 - 12
    
    // Позиция текста
    static constexpr uint16_t TEXT_X = 4;
    static constexpr uint16_t TEXT_Y = 2;  // Центрирование: (12-8)/2 = 2
    static constexpr uint8_t TEXT_SIZE = 1;  // 8px height per line
    
    // Цвета
    static constexpr uint16_t COLOR_BACKGROUND = TFT_BLACK;
    static constexpr uint16_t COLOR_TEXT = TFT_WHITE;

    /**
     * @brief Инициализация footer
     * @param tft Ссылка на TFT объект
     */
    void begin(TFT_eSPI& tft);
    
    /**
     * @brief Полная отрисовка footer (фон + текст)
     * @param moduleType Тип модуля (START/FINISH)
     * @param version Версия прошивки (например, "0.1.0")
     */
    void draw(ModuleType moduleType, const char* version);

private:
    TFT_eSPI* _tft = nullptr;
};

// Глобальный объект footer
extern Footer footer;

#endif // UI_FOOTER_H

