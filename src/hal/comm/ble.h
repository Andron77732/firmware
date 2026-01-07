#ifndef BLE_H
#define BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "config.h"

// Nordic UART Service UUIDs
#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHARACTERISTIC   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // Client -> ESP (Write)
#define NUS_TX_CHARACTERISTIC   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // ESP -> Client (Notify)

// 32KB буфер для больших пакетов настроек
#define BLE_RX_BUFFER_SIZE 32768

// Состояния Bluetooth
enum class BLEState {
    DISCONNECTED,   // Реклама не активна
    ADVERTISING,    // Реклама активна, но нет подключения
    CONNECTED       // Есть активное подключение
};

// Callback для уведомления об изменении состояния подключения
typedef void (*BLEStateCallback)(BLEState state);

// ============================================================================
// Plugin interface
// ============================================================================

/**
 * @brief Интерфейс BLE-плагина (сервиса).
 *
 * Плагин должен:
 *  - В init(server) создать свой service/characteristics и вызвать service->start()
 *  - НЕ вызывать NimBLEDevice::init(), НЕ стартовать рекламу
 *
 * BLESerial гарантирует, что init() всех плагинов будет вызван ДО startAdvertising().
 */
 class IBleServicePlugin {
    public:
        virtual ~IBleServicePlugin() = default;
    
        // Вызывается один раз перед стартом advertising.
        virtual void init(NimBLEServer* server) = 0;
    
        // Опциональные события
        virtual void onConnect(NimBLEConnInfo& /*connInfo*/, uint16_t /*mtu*/) {}
        virtual void onDisconnect(int /*reason*/) {}
        virtual void onMtuUpdated(uint16_t /*mtu*/) {}
    };

class BLESerial : public Stream {
public:

    void init(const String& deviceName = String(BLE_DEVICE_NAME));
    void startAdvertising();
    void stopAdvertising();
    void end();
    
    bool isConnected();
    bool isAdvertising();

    /**
    * @brief Доступ к серверу BLE для пользовательского добавления сервисов.
    * 
    * Метод позволяет получить указатель на внутренний NimBLEServer,
    * чтобы внешние модули могли добавлять пользовательские сервисы или характеристики.
    * @return NimBLEServer* Указатель на BLE сервер.
    */
    // NimBLEServer* getServer() const { return _server; }

    /**
     * @brief Регистрация BLE-плагина (сервиса).
     *
     * Важно:
     *  - вызывать после init()
     *  - и ДО startAdvertising()
     * @return true если зарегистрирован
     */
     bool registerService(IBleServicePlugin& plugin);
    
    /**
     * @brief Получить текущее состояние Bluetooth
     * @return BLEState Текущее состояние
     */
    BLEState getState();
    
    /**
     * @brief Установить callback для уведомления об изменении состояния подключения
     * @param callback Функция, которая будет вызвана при подключении/отключении с текущим состоянием
     */
    void setStateCallback(BLEStateCallback callback);
    
    // Stream interface
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    
    // Callback для внутреннего использования
    void onReceive(const uint8_t* data, size_t len);
    void onConnect(NimBLEConnInfo& connInfo, uint16_t mtu);
    void onDisconnect(int reason);
    void onMtuUpdated(uint16_t mtu);
    void onNotifyStateChanged(bool enabled);
    
private:
    void notifyStateChanged();
    void setupAdvertisingOnce();

    // Plugins
    void initPluginsOnce();
    void pluginsOnConnect(NimBLEConnInfo& connInfo, uint16_t mtu);
    void pluginsOnDisconnect(int reason);
    void pluginsOnMtuUpdated(uint16_t mtu);
    
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
    
    String _deviceName;
    // Callback для уведомления об изменении состояния
    BLEStateCallback _stateCallback = nullptr;

    bool _advConfigured = false; // Флаг для отслеживания настроенной рекламы

    // Plugin registry (фиксированный размер)
    static constexpr size_t MAX_PLUGINS = 8;
    IBleServicePlugin* _plugins[MAX_PLUGINS] = { nullptr };
    size_t _pluginCount = 0;
    bool _pluginsInited = false;
};

extern BLESerial bleSerial;

#endif // BLE_H

