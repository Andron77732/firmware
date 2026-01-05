#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "hal/comm/ble.h"
#include "hal/comm/wifi.h"
#include "ui_config.h"

/**
 * @brief Статус-бар в верхней части экрана
 * 
 * Layout (240px width):
 * ┌────────────────────────────────────────┐
 *  HH:MM:SS           [BT][WiFi][GPS][Bat]│
 * └────────────────────────────────────────┘
 * 
 * Левая половина (0-119): часы
 * Правая половина (120-239): иконки справа налево
 */
class StatusBar {
public:

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
    
    /**
     * @brief Обновить иконку Bluetooth в зависимости от состояния
     * @param state Текущее состояние Bluetooth
     */
    void updateBluetoothIcon(BLEState state);
    
    /**
     * @brief Обновить иконку WiFi в зависимости от состояния и уровня сигнала
     * @param state Текущее состояние WiFi
     * @param rssi Уровень сигнала в dBm (используется для выбора иконки при подключении)
     */
    void updateWiFiIcon(WiFiState state, int8_t rssi = 0);

private:
    TFT_eSPI* _tft = nullptr;
    
    // Кэш времени для оптимизации перерисовки
    uint8_t _lastHour = 255;
    uint8_t _lastMinute = 255;
    uint8_t _lastSecond = 255;
    
    // Кэш состояния Bluetooth для оптимизации перерисовки
    BLEState _lastBtState = BLEState::DISCONNECTED;
    
    // Кэш состояния WiFi для оптимизации перерисовки
    WiFiState _lastWiFiState = WiFiState::OFF;
    uint8_t _lastWiFiSignalLevel = 255; // 0-4 для уровней сигнала, 255 = не определен
    
    /**
     * @brief Отрисовка всех иконок
     */
    void drawIcons();
    
    void drawIconGPS(uint16_t color);
    void drawIconBluetooth(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconWiFi(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconBattery(uint16_t color);
    
    /**
     * @brief Отрисовка bitmap 16x16
     * @param bgColor Цвет фона
     */
    void drawBitmap16(uint16_t x, uint16_t y, const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
};

// Глобальный объект статус-бара
extern StatusBar statusBar;

#endif // UI_STATUS_BAR_H

