#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "hal/ble/ble.h"
#include "hal/wifi/wifi.h"
#include "hal/gps/gps.h"
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
     * @param time_sec Время в секундах (time_t)
     * @param timezone Часовой пояс (смещение в часах от UTC)
     */
    void updateTime(time_t time_sec, int8_t timezone);
    
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

    /**
     * @brief Обновить иконку GPS в зависимости от состояния
     * @param state Текущее состояние GPS
     */
    void updateGPSIcon(GPSState state);

private:
    TFT_eSPI* _tft = nullptr;
    
    // Кэш времени для оптимизации перерисовки
    time_t _lastTimeSec = 0;
    
    // Кэш состояния Bluetooth для оптимизации перерисовки
    BLEState _lastBtState = BLEState::DISCONNECTED;
    
    // Кэш состояния WiFi для оптимизации перерисовки
    WiFiState _lastWiFiState = WiFiState::UNINITIALIZED;
    uint8_t _lastWiFiSignalLevel = 255; // 0-4 для уровней сигнала, 255 = не определен

    // Кэш состояния GPS для оптимизации перерисовки
    GPSState _lastGpsState = GPSState::OFF;
    
    /**
     * @brief Отрисовка всех иконок
     */
    void drawIcons();
    
    void drawIconGPS(uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconBluetooth(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconWiFi(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconBattery(uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    
    /**
     * @brief Отрисовка bitmap 16x16
     * @param bgColor Цвет фона
     */
    void drawBitmap16(uint16_t x, uint16_t y, const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
};

// Глобальный объект статус-бара
extern StatusBar statusBar;

#endif // UI_STATUS_BAR_H
