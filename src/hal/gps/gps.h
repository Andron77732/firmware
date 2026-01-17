#ifndef HAL_GPS_H
#define HAL_GPS_H

#include <Arduino.h>
#include <MicroNMEA.h>

enum class GPSState : uint8_t {
  OFF = 0,
  SEARCHING = 1,
  ACTIVE = 2,
};

/**
 * @brief Класс GPS NEO-6M
 * 
 * Инкапсулирует HardwareSerial и MicroNMEA парсер.
 * Доступ к данным через nmea().
 * Пины конфигурируются через config.h
 * 
 * Сырые NMEA выводятся через ESP_LOGD (включить: CORE_DEBUG_LEVEL >= 4)
 */
class GPS {
public:
    /**
     * @brief Инициализация UART и парсера
     */
    void begin();
    
    /**
     * @brief Обработка входящих данных (вызывать в loop)
     */
    void update();
    
    /**
     * @brief Проверка инициализации
     */
    bool isReady() const { return _initialized; }
    
    /**
     * @brief Доступ к MicroNMEA объекту
     * 
     * Примеры:
     *   gps.nmea().isValid()
     *   gps.nmea().getHour()
     *   gps.nmea().getNumSatellites()
     */
    MicroNMEA& nmea() { return _nmea; }
    const MicroNMEA& nmea() const { return _nmea; }

    /**
     * @brief Время начала последнего полного NMEA-предложения (esp_timer_get_time, us)
     * @return true если есть валидная метка времени
     */
    bool lastSentenceStartUs(int64_t &ts_us) const;

    /**
     * @brief Получить текущее состояние GPS
     */
    GPSState getState() const;

    /**
     * @brief Время последнего валидного фикса (esp_timer_get_time, us)
     * @return 0 если фикса еще не было
     */
    int64_t lastFixUs() const { return _last_fix_us; }

    /**
     * @brief Установить callback для уведомления об изменении состояния GPS
     * @param callback Функция, которая будет вызвана при изменении состояния
     */
    void setStateCallback(void (*callback)(GPSState state));
    
    /**
     * @brief Установить callback для уведомления об изменении числа спутников
     * @param callback Функция, которая будет вызвана при изменении количества спутников
     */
    void setSatsCallback(void (*callback)(int8_t sats));

private:
    static constexpr size_t NMEA_BUFFER_SIZE = 128;
    
    char _nmeaBuffer[NMEA_BUFFER_SIZE];
    MicroNMEA _nmea{_nmeaBuffer, NMEA_BUFFER_SIZE};
    
    bool _initialized = false;

    // Callback для уведомления об изменении состояния
    void (*_stateCallback)(GPSState state) = nullptr;
    GPSState _lastState = GPSState::OFF;

    // Callback для уведомления об изменении числа спутников
    void (*_satsCallback)(int8_t sats) = nullptr;
    int8_t _lastSats = -127;

    int64_t _last_fix_us = 0;

    // Мягкий таймстампинг NMEA: время начала последнего полного предложения
    int64_t _last_sentence_start_us = 0;
    int64_t _current_sentence_start_us = 0;
    bool _in_sentence = false;

    void notifyStateChanged_();
    void updateState_();
    void notifySatsChanged_();
    void updateSats_();
    int8_t currentSats_() const;
};

// Глобальный объект GPS
extern GPS gps;

#endif // HAL_GPS_H
