#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "hal/ble/ble.h"
#include "hal/wifi/wifi.h"
#include "hal/gps/gps.h"
#include "hal/touch/touch.h"
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
    void init(TFT_eSPI& tft);
    
    /**
     * @brief Полная отрисовка статус-бара (фон + все элементы)
     */
    void draw();
    
    /**
     * @brief Обновить время на статус-баре
     * @param local_tm Локальное время (tm, уже с учетом таймзоны)
     */
    void updateTime(const struct tm &local_tm);
    
    /**
     * @brief Принудительная перерисовка времени (даже если не изменилось)
     */
    void drawTimePlaceholder();
    
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

    /**
     * @brief Обновить иконку батареи по напряжению
     * @param voltage Напряжение батареи (В)
     * @param valid Есть ли валидные данные INA226
     */
    void updateBatteryVoltage(float voltage, bool valid);

    /**
     * @brief Обработчик touch-событий
     * @return true если событие обработано
     */
    bool onTouchEvent(const TouchEvent& event);

private:
    enum class BatteryLevel : uint8_t {
        Empty = 0,
        Low = 1,
        Mid = 2,
        Full = 3,
    };

    TFT_eSPI* _tft = nullptr;
    BLEState _btState = BLEState::DISCONNECTED;
    WiFiState _wifiState = WiFiState::UNINITIALIZED;
    uint8_t _wifiSignalLevel = 255; // 0-4 для уровней сигнала, 255 = не определен
    GPSState _gpsState = GPSState::OFF;
    bool _batteryValid = false;
    float _batteryVoltage = NAN;
    BatteryLevel _batteryLevel = BatteryLevel::Empty;
    bool _hasTime = false;
    tm _time = {};

    uint8_t wifiSignalLevelFromRssi(WiFiState state, int8_t rssi) const;
    void drawTimeValue();
    void drawBluetoothValue(BLEState state);
    void drawWiFiValue(WiFiState state, uint8_t signalLevel);
    void drawGpsValue(GPSState state);
    void drawBatteryValue(bool valid, BatteryLevel level);
    BatteryLevel batteryLevelFromVoltage(float voltage) const;
    
    void drawIconGPS(uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconBluetooth(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconWiFi(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    void drawIconBattery(const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
    
    /**
     * @brief Отрисовка bitmap 16x16
     * @param bgColor Цвет фона
     */
    void drawBitmap16(uint16_t x, uint16_t y, const uint8_t* bitmap, uint16_t color, uint16_t bgColor = UI_STATUS_BAR_COLOR_BACKGROUND);
};

#endif // UI_STATUS_BAR_H
