#ifndef UI_FOOTER_H
#define UI_FOOTER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "ui_config.h"

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

