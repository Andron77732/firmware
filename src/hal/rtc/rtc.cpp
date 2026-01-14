#include "rtc.h"
#include "config.h"
#include "esp_log.h"
#include <Wire.h>

static const char* TAG = "RTC";

// Глобальный объект RTC
RTC rtc;

bool RTC::begin() {
    if (_initialized) return true;
    
    // Инициализация I2C на заданных пинах
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    if (!_rtc.begin(&Wire)) {
        ESP_LOGE(TAG, "DS3231 not found on I2C (SDA:%d, SCL:%d)", I2C_SDA_PIN, I2C_SCL_PIN);
        return false;
    }
    
    ESP_LOGI(TAG, "Initialized (SDA:%d, SCL:%d, SQW:%d)", I2C_SDA_PIN, I2C_SCL_PIN, RTC_SQW_PIN);
    
    // Настройка SQW пина: 1 Hz square wave для синхронизации
    pinMode(RTC_SQW_PIN, INPUT_PULLUP);
    _rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
    ESP_LOGI(TAG, "SQW configured: 1 Hz");
    
    // Проверка потери питания
    if (_rtc.lostPower()) {
        ESP_LOGW(TAG, "Lost power detected! Time may be invalid.");
    }
    
    // Логирование текущего времени
    DateTime now = _rtc.now();
    ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d", 
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    
    // Температура чипа
    float temp = _rtc.getTemperature();
    ESP_LOGI(TAG, "Chip temperature: %.2f °C", temp);
    
    _initialized = true;
    return true;
}

bool RTC::lostPower() {
    if (!_initialized) return true;
    return _rtc.lostPower();
}

DateTime RTC::now() {
    if (!_initialized) {
        ESP_LOGW(TAG, "RTC not initialized, returning epoch");
        return DateTime((uint32_t)0);
    }
    return _rtc.now();
}

uint32_t RTC::unixTime() {
    return now().unixtime();
}

void RTC::setTime(const DateTime& dt) {
    if (!_initialized) {
        ESP_LOGE(TAG, "Cannot set time: RTC not initialized");
        return;
    }
    
    _rtc.adjust(dt);
    
    ESP_LOGI(TAG, "Time set to: %04d-%02d-%02d %02d:%02d:%02d",
             dt.year(), dt.month(), dt.day(),
             dt.hour(), dt.minute(), dt.second());
}

void RTC::setTime(uint32_t unixtime) {
    setTime(DateTime(unixtime));
}

void RTC::setTime(uint16_t year, uint8_t month, uint8_t day,
                  uint8_t hour, uint8_t minute, uint8_t second) {
    if (!_initialized) {
        ESP_LOGE(TAG, "Cannot set time: RTC not initialized");
        return;
    }
    
    DateTime dt(year, month, day, hour, minute, second);
    _rtc.adjust(dt);
    
    ESP_LOGI(TAG, "Time set to: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
}

float RTC::getTemperature() {
    if (!_initialized) {
        ESP_LOGW(TAG, "RTC not initialized");
        return 0.0f;
    }
    return _rtc.getTemperature();
}
