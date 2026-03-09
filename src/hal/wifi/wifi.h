#ifndef WIFI_H
#define WIFI_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_log.h"
#include "config.h"

// Состояния WiFi
enum class WiFiState : uint8_t {
    UNINITIALIZED = 0, // WiFiManager не инициализирован
    OFF = 1,           // WiFi выключен (после begin/end)
    CONNECTING = 2,    // Идет подключение
    CONNECTED = 3,     // Подключено к сети
    DISCONNECTED = 4,  // Не подключено (но WiFi включен)
    RECONNECTING = 5,  // Ожидание/выполнение попытки переподключения
    ERROR = 6,         // Ошибка подключения
};

// Callback для уведомления об изменении состояния подключения
// Параметры: состояние, уровень сигнала RSSI
typedef void (*WiFiStateCallback)(WiFiState state, int8_t rssi);

class WiFiManager {
public:
    /**
     * @brief Инициализация WiFi модуля
     */
    void begin();
    
    /**
     * @brief Отключение WiFi модуля
     */
    bool end();
    
    /**
     * @brief Подключение к WiFi сети
     * @param ssid SSID сети
     * @param password Пароль сети (может быть пустым для открытых сетей)
     * @return true если подключение начато, false при ошибке
     */
    bool connect(const String& ssid, const String& password = "");
    
    /**
     * @brief Отключение от WiFi сети
     */
    void disconnect();
    
    /**
     * @brief Проверка подключения к сети
     * @return true если подключено и получен IP
     */
    bool isConnected();
    
    /**
     * @brief Получить текущее состояние WiFi
     * @return WiFiState Текущее состояние
     */
    WiFiState getState();
    
    /**
     * @brief Установить callback для уведомления об изменении состояния подключения
     * @param callback Функция, которая будет вызвана при изменении состояния
     */
    void setStateCallback(WiFiStateCallback callback);
    
    /**
     * @brief Обновление состояния WiFi (вызывается в loop)
     * Обрабатывает события, обновляет RSSI, управляет переподключением
     */
    void update();
    
    /**
     * @brief Получить уровень сигнала (RSSI)
     * @return RSSI в dBm, или 0 если не подключено
     */
    int8_t getRSSI();
    
    /**
     * @brief Получить IP адрес
     * @return IP адрес в виде строки, или пустая строка если не подключено
     */
    String getIP();

    /**
     * @brief Получить SSID текущей сети
     * @return SSID, или пустая строка если не подключено
     */
    String getSSID();
    
    /**
     * @brief Внутренний callback для обработки событий WiFi
     */
    void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info);
    
private:
    WiFiState _state = WiFiState::UNINITIALIZED;
    bool _autoReconnect = true;
    bool _eventRegistered = false;
    wifi_event_id_t _eventHandlerId = 0;
    uint32_t _lastReconnectAttempt = 0;
    uint32_t _reconnectInterval = 5000; // 5 секунд между попытками
    uint8_t _reconnectAttempts = 0;
    const uint8_t _maxReconnectAttempts = 10;
    
    String _ssid;
    String _password;
    
    int8_t _rssi = 0;
    uint32_t _lastRSSIUpdate = 0;
    const uint32_t _rssiUpdateInterval = 1000; // Обновлять RSSI раз в секунду
    
    // Callback для уведомления об изменении состояния
    WiFiStateCallback _stateCallback = nullptr;
    
    /**
     * @brief Установить состояние и уведомить callback
     */
    void setState(WiFiState newState);
    
    /**
     * @brief Попытка переподключения
     */
    void attemptReconnect();
};

// Глобальный экземпляр
extern WiFiManager wifiManager;

#endif // WIFI_H
