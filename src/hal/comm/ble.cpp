#include "ble.h"
#include "esp_log.h"

static const char* TAG = "BLE";

// Глобальный экземпляр
BLESerial bleSerial;

// ============================================================================
// Callbacks
// ============================================================================

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        uint16_t mtu = connInfo.getMTU();
        ESP_LOGI(TAG, "Client connected: %s, MTU: %d", connInfo.getAddress().toString().c_str(), mtu);
        bleSerial.onConnect(mtu);
    }
    
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        ESP_LOGI(TAG, "Client disconnected, reason: %d", reason);
        bleSerial.onDisconnect();
        // Перезапуск рекламы
        NimBLEDevice::getAdvertising()->start();
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            bleSerial.onReceive((const uint8_t*)value.data(), value.length());
        }
    }
};

class TxCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        bool notify = (subValue & 0x01) != 0;
        ESP_LOGI(TAG, "Notify %s", notify ? "enabled" : "disabled");
        bleSerial.onNotifyStateChanged(notify);
    }
};

// ============================================================================
// BLESerial Implementation
// ============================================================================

void BLESerial::begin(const char* deviceName) {
    ESP_LOGI(TAG, "Initializing NUS as '%s'...", deviceName);
    
    // Создание StreamBuffer для RX (thread-safe)
    _rxStream = xStreamBufferCreate(BLE_RX_BUFFER_SIZE, 1);
    if (!_rxStream) {
        ESP_LOGE(TAG, "Failed to create RX StreamBuffer");
        return;
    }
    ESP_LOGI(TAG, "RX StreamBuffer created: %d bytes", BLE_RX_BUFFER_SIZE);
    
    // Инициализация NimBLE
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setMTU(247);
    
    // Создание сервера
    _server = NimBLEDevice::createServer();
    _server->setCallbacks(new ServerCallbacks());
    
    // Создание NUS сервиса
    _service = _server->createService(NUS_SERVICE_UUID);
    
    // RX характеристика (клиент пишет сюда)
    _rxCharacteristic = _service->createCharacteristic(
        NUS_RX_CHARACTERISTIC,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _rxCharacteristic->setCallbacks(new RxCallbacks());
    
    // TX характеристика (ESP отправляет через notify)
    _txCharacteristic = _service->createCharacteristic(
        NUS_TX_CHARACTERISTIC,
        NIMBLE_PROPERTY::NOTIFY
    );
    _txCharacteristic->setCallbacks(new TxCallbacks());
    
    // Запуск сервиса
    _service->start();
    
    // Настройка и запуск рекламы
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName(deviceName);
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    pAdvertising->start();
    
    ESP_LOGI(TAG, "NUS started, advertising...");
}

void BLESerial::end() {
    NimBLEDevice::deinit(true);
    _server = nullptr;
    _service = nullptr;
    _rxCharacteristic = nullptr;
    _txCharacteristic = nullptr;
    _connected = false;
    
    // Освобождение StreamBuffer
    if (_rxStream) {
        vStreamBufferDelete(_rxStream);
        _rxStream = nullptr;
    }
    _hasPeeked = false;
    _peekedByte = -1;
}

bool BLESerial::isConnected() {
    return _connected;
}

// ============================================================================
// Stream Interface
// ============================================================================

int BLESerial::available() {
    if (!_rxStream) return 0;
    size_t avail = xStreamBufferBytesAvailable(_rxStream);
    return avail + (_hasPeeked ? 1 : 0);
}

int BLESerial::read() {
    // Если есть peeked байт, вернуть его
    if (_hasPeeked) {
        _hasPeeked = false;
        return _peekedByte;
    }
    
    if (!_rxStream) return -1;
    
    uint8_t c;
    if (xStreamBufferReceive(_rxStream, &c, 1, 0) == 1) {
        return c;
    }
    return -1;
}

int BLESerial::peek() {
    // Если уже есть peeked байт, вернуть его
    if (_hasPeeked) {
        return _peekedByte;
    }
    
    if (!_rxStream) return -1;
    
    // Прочитать байт и сохранить для следующего read()
    uint8_t c;
    if (xStreamBufferReceive(_rxStream, &c, 1, 0) == 1) {
        _peekedByte = c;
        _hasPeeked = true;
        return c;
    }
    return -1;
}

void BLESerial::flush() {
    // Для TX — ничего не делаем, notify отправляется сразу
}

size_t BLESerial::write(uint8_t c) {
    return write(&c, 1);
}

size_t BLESerial::write(const uint8_t* buffer, size_t size) {
    if (!_connected || !_txCharacteristic || !_notifyEnabled) {
        return 0;
    }
    
    // Используем согласованный MTU минус 3 байта ATT header
    const size_t maxChunk = (_mtu > 3) ? (_mtu - 3) : 20;
    const int maxRetries = 5;
    size_t sent = 0;
    
    while (sent < size) {
        size_t chunk = min(maxChunk, size - sent);
        _txCharacteristic->setValue(buffer + sent, chunk);
        
        // Retry loop для congestion control
        int retries = 0;
        while (!_txCharacteristic->notify()) {
            retries++;
            if (retries >= maxRetries) {
                ESP_LOGW(TAG, "Notify failed after %d retries, sent %d/%d bytes", maxRetries, sent, size);
                return sent;  // Вернуть сколько успели отправить
            }
            ESP_LOGD(TAG, "Notify congestion, retry %d", retries);
            vTaskDelay(pdMS_TO_TICKS(10));
            
            // Проверка что всё ещё подключены
            if (!_connected || !_notifyEnabled) {
                return sent;
            }
        }
        
        sent += chunk;
        
        // Небольшая задержка между чанками для медленных клиентов
        if (sent < size) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    
    return sent;
}

// ============================================================================
// Internal Callbacks
// ============================================================================

void BLESerial::onReceive(const uint8_t* data, size_t len) {
    if (!_rxStream) return;
    
    size_t sent = xStreamBufferSend(_rxStream, data, len, 0);
    if (sent < len) {
        // Rate limit overflow warnings (max once per second)
        static uint32_t lastOverflowLog = 0;
        static size_t overflowCount = 0;
        uint32_t now = xTaskGetTickCount();
        
        overflowCount++;
        if (now - lastOverflowLog > pdMS_TO_TICKS(1000)) {
            ESP_LOGW(TAG, "RX buffer overflow: lost %d bytes (%d events)", len - sent, overflowCount);
            lastOverflowLog = now;
            overflowCount = 0;
        }
    }
}

void BLESerial::onConnect(uint16_t mtu) {
    _connected = true;
    _mtu = mtu;
}

void BLESerial::onDisconnect() {
    _connected = false;
    _notifyEnabled = false;
}

void BLESerial::onNotifyStateChanged(bool enabled) {
    _notifyEnabled = enabled;
}

