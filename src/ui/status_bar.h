#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include <Arduino.h>
#include <TFT_eSPI.h>

/**
 * @brief Статус-бар в верхней части экрана
 * 
 * Layout (240px width):
 * ┌──────────────────────────────────────┐
 * │ HH:MM:SS           [BT][WiFi][GPS][Bat]│
 * └──────────────────────────────────────┘
 * 
 * Левая половина (0-119): часы
 * Правая половина (120-239): иконки справа налево
 */
class StatusBar {
public:
    // Размеры и позиции
    static constexpr uint16_t WIDTH = 240;
    static constexpr uint16_t HEIGHT = 24;  // 4 + 16 + 4
    static constexpr uint16_t Y_POS = 0;
    
    // Зона часов (TextSize 2 = 16px height)
    static constexpr uint16_t CLOCK_X = 4;
    static constexpr uint16_t CLOCK_Y = 4;
    static constexpr uint8_t  CLOCK_TEXT_SIZE = 2;  // 12x16 per char, "HH:MM:SS" = 96px
    static constexpr uint16_t CLOCK_HEIGHT = 16;    // 8 * CLOCK_TEXT_SIZE
    
    // Зона иконок (справа налево, от правого края)
    static constexpr uint16_t ICON_SIZE = CLOCK_HEIGHT;  // 16px — равно высоте часов
    static constexpr uint16_t ICON_PADDING = 4;
    static constexpr uint16_t ICON_Y = CLOCK_Y;  // выровнено с часами
    
    // Позиции иконок (X координата левого края, справа налево)
    static constexpr uint16_t ICON_BATTERY_X   = WIDTH - ICON_PADDING - ICON_SIZE;              // 220
    static constexpr uint16_t ICON_WIFI_X      = ICON_BATTERY_X - ICON_PADDING - ICON_SIZE;     // 200
    static constexpr uint16_t ICON_BLUETOOTH_X = ICON_WIFI_X - ICON_PADDING - ICON_SIZE;        // 180
    static constexpr uint16_t ICON_GPS_X       = ICON_BLUETOOTH_X - ICON_PADDING - ICON_SIZE;   // 160
    
    // Цвета
    static constexpr uint16_t COLOR_BACKGROUND = TFT_BLACK;
    static constexpr uint16_t COLOR_CLOCK = TFT_WHITE;
    static constexpr uint16_t COLOR_ICON_ACTIVE = TFT_WHITE;
    static constexpr uint16_t COLOR_ICON_INACTIVE = TFT_DARKGREY;

    /**
     * @brief Инициализация статус-бара
     * @param tft Ссылка на TFT объект
     */
    void begin(TFT_eSPI& tft);
    
    /**
     * @brief Полная отрисовка статус-бара (фон + все элементы)
     */
    void draw();
    
    /**
     * @brief Обновить время на статус-баре
     * @param hour Час (0-23)
     * @param minute Минута (0-59)
     * @param second Секунда (0-59)
     */
    void updateTime(uint8_t hour, uint8_t minute, uint8_t second);
    
    /**
     * @brief Принудительная перерисовка времени (даже если не изменилось)
     */
    void forceRedrawTime();

private:
    TFT_eSPI* _tft = nullptr;
    
    // Кэш времени для оптимизации перерисовки
    uint8_t _lastHour = 255;
    uint8_t _lastMinute = 255;
    uint8_t _lastSecond = 255;
    
    /**
     * @brief Отрисовка всех иконок
     */
    void drawIcons();
    
    void drawIconGPS(uint16_t color);
    void drawIconBluetooth(uint16_t color);
    void drawIconWiFi(uint16_t color);
    void drawIconBattery(uint16_t color);
    
    /**
     * @brief Отрисовка bitmap 16x16
     */
    void drawBitmap16(uint16_t x, uint16_t y, const uint8_t* bitmap, uint16_t color);
};

// Глобальный объект статус-бара
extern StatusBar statusBar;

#endif // UI_STATUS_BAR_H

