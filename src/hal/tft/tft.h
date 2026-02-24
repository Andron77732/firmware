#ifndef HAL_TFT_H
#define HAL_TFT_H

#include <TFT_eSPI.h>

/**
 * @brief Класс дисплея ILI9341
 * 
 * Композиция с TFT_eSPI, доступ через tft().
 * Пины конфигурируются через build_flags в platformio.ini
 */
class Display {
public:
    /**
     * @brief Инициализация дисплея
     * @param rotation Ориентация (0-3)
     */
    void begin(uint8_t rotation);
    
    /**
     * @brief Очистить экран
     * @param color Цвет заливки (по умолчанию чёрный)
     */
    void clear(uint16_t color = TFT_BLACK);
    
    /**
     * @brief Проверка инициализации
     */
    bool isReady() const { return _initialized; }
    
    /**
     * @brief Доступ к TFT_eSPI объекту
     */
    TFT_eSPI& tft() { return _tft; }

private:
    TFT_eSPI _tft;
    bool _initialized = false;
};

// Глобальный объект дисплея
extern Display display;

#endif // HAL_TFT_H
