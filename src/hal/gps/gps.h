#ifndef HAL_GPS_H
#define HAL_GPS_H

#include <Arduino.h>
#include <MicroNMEA.h>

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

private:
    static constexpr size_t NMEA_BUFFER_SIZE = 128;
    
    char _nmeaBuffer[NMEA_BUFFER_SIZE];
    MicroNMEA _nmea{_nmeaBuffer, NMEA_BUFFER_SIZE};
    
    bool _initialized = false;
};

// Глобальный объект GPS
extern GPS gps;

#endif // HAL_GPS_H
