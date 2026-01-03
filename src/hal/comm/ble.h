#ifndef BLE_H
#define BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "esp_heap_caps.h"
#include "config.h"

// Nordic UART Service UUIDs
#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Client -> ESP (Write)
#define NUS_TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP -> Client (Notify)

// 32KB буфер в PSRAM для больших пакетов настроек
#define BLE_RX_BUFFER_SIZE 32768

class BLESerial : public Stream {
public:
    void begin(const char* deviceName = BLE_DEVICE_NAME);
    void end();
    
    bool isConnected();
    
    // Stream interface
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    
    // Callback для внутреннего использования
    void onReceive(const uint8_t* data, size_t len);
    void onConnect(uint16_t mtu);
    void onDisconnect();
    void onNotifyStateChanged(bool enabled);
    
private:
    NimBLEServer* _server = nullptr;
    NimBLEService* _service = nullptr;
    NimBLECharacteristic* _rxCharacteristic = nullptr;
    NimBLECharacteristic* _txCharacteristic = nullptr;
    
    // Thread-safe RX buffer (FreeRTOS StreamBuffer)
    StreamBufferHandle_t _rxStream = nullptr;
    
    // Для peek() - StreamBuffer не поддерживает peek напрямую
    int _peekedByte = -1;
    bool _hasPeeked = false;
    
    bool _connected = false;
    bool _notifyEnabled = false;  // Клиент подписан на notify
    uint16_t _mtu = 23;  // Минимальный BLE MTU по умолчанию
};

extern BLESerial bleSerial;

#endif // BLE_H

