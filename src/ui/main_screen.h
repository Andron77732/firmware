#ifndef UI_MAIN_SCREEN_H
#define UI_MAIN_SCREEN_H

#include "footer.h"
#include "hal/ina226/ina226.h"
#include "main_area.h"
#include "status_bar.h"
#include <freertos/FreeRTOS.h>

/**
 * @brief Координатор главного экрана
 */
class MainScreen {
public:
    /**
     * @brief Инициализация зависимостей главного экрана
     */
    void init(Footer& footer, MainArea& mainArea, StatusBar& statusBar);

    /**
     * @brief Полная отрисовка главного экрана
     */
    void draw();

    /**
     * @brief Обновление состояния главного экрана
     */
    void update();

    void postBleState(BLEState state);
    void postWiFiState(WiFiState state, int8_t rssi);
    void postGpsState(GPSState state);
    void postTimeSyncState(TimeSyncState state);
    void postSats(int8_t sats);
    void postBatteryLevel(InaBatteryLevel level);

private:
    enum DirtyBit : uint32_t {
        DIRTY_BLE = 1u << 0,
        DIRTY_WIFI = 1u << 1,
        DIRTY_GPS = 1u << 2,
        DIRTY_TIME_SYNC = 1u << 3,
        DIRTY_SATS = 1u << 4,
        DIRTY_BATTERY_LEVEL = 1u << 5,
    };

    struct PendingState {
        BLEState bleState = BLEState::DISCONNECTED;
        WiFiState wifiState = WiFiState::UNINITIALIZED;
        int8_t wifiRssi = 0;
        GPSState gpsState = GPSState::OFF;
        TimeSyncState timeSyncState = TimeSyncState::NONE;
        int8_t sats = -1;
        InaBatteryLevel batteryLevel = InaBatteryLevel::NoData;
        uint32_t dirtyMask = 0;
    };

    Footer* footer = nullptr;
    MainArea* mainArea = nullptr;
    StatusBar* statusBar = nullptr;
    bool _initialized = false;
    portMUX_TYPE _pendingMux = portMUX_INITIALIZER_UNLOCKED;
    PendingState _pending;
};

#endif // UI_MAIN_SCREEN_H
