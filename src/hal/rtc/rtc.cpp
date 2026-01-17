#include "rtc.h"
#include "config.h"
#include "esp_log.h"
#include <Wire.h>
#include <cmath>
#include <esp_timer.h>

static const char* TAG = "RTC";
static const uint8_t kDs3231Address = 0x68;
static const uint8_t kDs3231AgingOffsetReg = 0x10;
static const float kDs3231AgingStepPpm = 0.1f;

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
    _last_sync_us = esp_timer_get_time();
    
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
    _last_sync_us = esp_timer_get_time();
    
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

bool RTC::getAgingOffsetPpm(float &ppm) {
    if (!_initialized) {
        ESP_LOGW(TAG, "RTC not initialized");
        return false;
    }

    int8_t raw = 0;
    if (!readAgingOffsetRaw_(raw)) {
        ESP_LOGW(TAG, "Failed to read DS3231 aging offset");
        return false;
    }

    ppm = static_cast<float>(raw) * kDs3231AgingStepPpm;
    return true;
}

bool RTC::setAgingOffsetPpm(float ppm, float &applied_ppm) {
    if (!_initialized) {
        ESP_LOGW(TAG, "RTC not initialized");
        return false;
    }

    int raw = lroundf(ppm / kDs3231AgingStepPpm);
    if (raw > 127) raw = 127;
    if (raw < -128) raw = -128;

    if (!writeAgingOffsetRaw_(static_cast<int8_t>(raw))) {
        ESP_LOGW(TAG, "Failed to write DS3231 aging offset");
        return false;
    }

    applied_ppm = static_cast<float>(raw) * kDs3231AgingStepPpm;
    return true;
}

bool RTC::readAgingOffsetRaw_(int8_t &raw) {
    Wire.beginTransmission(kDs3231Address);
    Wire.write(kDs3231AgingOffsetReg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(kDs3231Address, static_cast<uint8_t>(1)) != 1) {
        return false;
    }

    raw = static_cast<int8_t>(Wire.read());
    return true;
}

bool RTC::writeAgingOffsetRaw_(int8_t raw) {
    Wire.beginTransmission(kDs3231Address);
    Wire.write(kDs3231AgingOffsetReg);
    Wire.write(static_cast<uint8_t>(raw));
    return Wire.endTransmission() == 0;
}
