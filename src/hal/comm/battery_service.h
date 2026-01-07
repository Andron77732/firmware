#pragma once
#include <NimBLEDevice.h>

class BleBatteryService {
public:
    void init(NimBLEServer* server);
    void setLevel(uint8_t percent);

    // вызывается из callback
    void onNotifyStateChanged(bool enabled);

private:
    NimBLEService* _service = nullptr;
    NimBLECharacteristic* _levelChar = nullptr;
    uint8_t _lastLevel = 255;
    bool _notifyEnabled = false;  // Клиент подписан на notify
};

extern BleBatteryService batteryService;