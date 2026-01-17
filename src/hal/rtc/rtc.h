#ifndef HAL_RTC_H
#define HAL_RTC_H

#include <Arduino.h>
#include <RTClib.h>

/**
 * @brief Класс RTC DS3231
 * 
 * Инкапсулирует I2C коммуникацию с DS3231.
 * Резервный источник времени при отсутствии GPS.
 * Пины конфигурируются через config.h
 * 
 * DS3231 особенности:
 *   - Точность ±2ppm (±0.17 сек/день)
 *   - Встроенный температурный сенсор для компенсации
 *   - Батарейный backup
 */
class RTC {
public:
    /**
     * @brief Инициализация I2C и DS3231
     * @return true если RTC найден и работает
     */
    bool begin();
    
    /**
     * @brief Проверка инициализации
     */
    bool isReady() const { return _initialized; }
    
    /**
     * @brief Проверка потери питания (требуется установка времени)
     */
    bool lostPower();
    
    /**
     * @brief Получить текущее время
     * @return DateTime объект с текущим временем
     */
    DateTime now();
    
    /**
     * @brief Получить время в Unix timestamp (секунды с 1970)
     */
    uint32_t unixTime();
    
    /**
     * @brief Установить время
     * @param dt DateTime объект
     */
    void setTime(const DateTime& dt);
    
    /**
     * @brief Установить время из Unix timestamp
     * @param unixtime секунды с 1970-01-01 00:00:00 UTC
     */
    void setTime(uint32_t unixtime);
    
    /**
     * @brief Установить время из компонентов
     * @param year Год (полный, например 2024)
     * @param month Месяц (1-12)
     * @param day День (1-31)
     * @param hour Час (0-23)
     * @param minute Минута (0-59)
     * @param second Секунда (0-59)
     */
    void setTime(uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour, uint8_t minute, uint8_t second);
    
    /**
     * @brief Получить температуру чипа DS3231
     * @return Температура в °C (точность ±3°C)
     */
    float getTemperature();

    /**
     * @brief Получить смещение RTC (ppm), записанное в DS3231 aging offset
     * @return true если удалось прочитать
     */
    bool getAgingOffsetPpm(float &ppm);

    /**
     * @brief Установить смещение RTC (ppm) в DS3231 aging offset
     * @return true если запись успешна
     */
    bool setAgingOffsetPpm(float ppm, float &applied_ppm);

    /**
     * @brief Время последней установки RTC (esp_timer_get_time, us)
     * @return 0 если время еще ни разу не устанавливалось
     */
    int64_t lastSyncUs() const { return _last_sync_us; }
    
    /**
     * @brief Доступ к RTC_DS3231 объекту напрямую
     */
    RTC_DS3231& rtc() { return _rtc; }
    const RTC_DS3231& rtc() const { return _rtc; }

private:
    RTC_DS3231 _rtc;
    bool _initialized = false;
    int64_t _last_sync_us = 0;

    bool readAgingOffsetRaw_(int8_t &raw);
    bool writeAgingOffsetRaw_(int8_t raw);
};

// Глобальный объект RTC
extern RTC rtc;

#endif // HAL_RTC_H
