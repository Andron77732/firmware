#include "ina226.h"

#include "config.h"
#include "esp_log.h"

#include <Wire.h>
#include <cmath>
#include <cstdio>
#include <cstring>

static const char* TAG = "INA226";

Ina226Hal ina226;
volatile bool Ina226Hal::_dataReadyFlag = false;

Ina226Hal::Ina226Hal() : _sensor(INA226_I2C_ADDRESS, &Wire) {}

bool Ina226Hal::begin() {
    if (_initialized) {
        return true;
    }

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!_sensor.begin()) {
        setError_("INA226 not found on I2C");
        ESP_LOGE(TAG, "%s (SDA:%d, SCL:%d, addr:0x%02X)", _lastError,
                 I2C_SDA_PIN, I2C_SCL_PIN, INA226_I2C_ADDRESS);
        return false;
    }

    const int calibrationError =
        _sensor.setMaxCurrentShunt(INA226_MAX_CURRENT_A, INA226_SHUNT_OHMS);
    if (calibrationError != INA226_ERR_NONE) {
        snprintf(_lastError, ERROR_BUFFER_SIZE,
                 "Calibration failed, err=%d", calibrationError);
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setModeShuntBusContinuous()) {
        setError_("Failed to set continuous mode");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setAlertPolarity(false)) {
        setError_("Failed to set alert polarity");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setAlertLatchEnable(false)) {
        setError_("Failed to set alert latch mode");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setAlertRegister(INA226_CONVERSION_READY)) {
        setError_("Failed to set alert type");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    pinMode(INA226_ALERT_PIN, INPUT_PULLUP);
    int irq = digitalPinToInterrupt(INA226_ALERT_PIN);
    if (irq < 0) {
        setError_("INA226 alert pin has no interrupt");
        ESP_LOGE(TAG, "%s: GPIO%d", _lastError, INA226_ALERT_PIN);
        return false;
    }

    _initialized = true;
    _hasValidSample = false;
    _busVoltage = NAN;
    _current = NAN;
    _power = NAN;
    _batteryLevel = InaBatteryLevel::NoData;
    _dataReadyFlag = false;

    attachInterrupt(irq, Ina226Hal::onAlertIsr, FALLING);

    clearError_();

    ESP_LOGI(TAG,
             "Initialized (SDA:%d, SCL:%d, addr:0x%02X, alert:%d, shunt=%.6f, "
             "maxCurrent=%.3f)",
             I2C_SDA_PIN, I2C_SCL_PIN, INA226_I2C_ADDRESS, INA226_ALERT_PIN,
             INA226_SHUNT_OHMS, INA226_MAX_CURRENT_A);
    return true;
}

void Ina226Hal::setLevelChangedCallback(InaLevelChangedCallback callback) {
    _levelChangedCallback = callback;
}

void Ina226Hal::update() {
    if (!_initialized) {
        return;
    }

    bool pending = false;
    noInterrupts();
    pending = _dataReadyFlag;
    _dataReadyFlag = false;
    interrupts();

    if (!pending) {
        return;
    }

    processDataReady_();
}

void Ina226Hal::processDataReady_() {
    if (!_initialized) {
        return;
    }

    if (!_sensor.isConversionReady()) {
        return;
    }

    const float busVoltage = _sensor.getBusVoltage();
    const float current = _sensor.getCurrent();
    const float power = _sensor.getPower();
    const int readError = _sensor.getLastError();

    if (readError != INA226_ERR_NONE || !std::isfinite(busVoltage) ||
        !std::isfinite(current) || !std::isfinite(power)) {
        snprintf(_lastError, ERROR_BUFFER_SIZE,
                 "Read failed, err=%d, V=%f, I=%f, P=%f", readError, busVoltage,
                 current, power);
        _hasValidSample = false;
        _busVoltage = NAN;
        _current = NAN;
        _power = NAN;
        publishLevelIfChanged_(InaBatteryLevel::NoData);
        ESP_LOGW(TAG, "%s", _lastError);
        return;
    }

    _busVoltage = busVoltage;
    _current = current;
    _power = power;
    _hasValidSample = true;
    clearError_();

    const InaBatteryLevel nextLevel = applyHysteresis_(busVoltage);
    publishLevelIfChanged_(nextLevel);
}

InaBatteryLevel Ina226Hal::levelFromVoltage_(float voltage) const {
    if (voltage < INA226_BAT_EMPTY_MAX_V) {
        return InaBatteryLevel::Empty;
    }
    if (voltage < INA226_BAT_LOW_MAX_V) {
        return InaBatteryLevel::Low;
    }
    if (voltage < INA226_BAT_MID_MAX_V) {
        return InaBatteryLevel::Mid;
    }
    return InaBatteryLevel::Full;
}

InaBatteryLevel Ina226Hal::applyHysteresis_(float voltage) const {
    const float h = INA226_BAT_HYSTERESIS_V;

    switch (_batteryLevel) {
        case InaBatteryLevel::NoData:
            return levelFromVoltage_(voltage);
        case InaBatteryLevel::Empty:
            return (voltage >= (INA226_BAT_EMPTY_MAX_V + h)) ? InaBatteryLevel::Low
                                                             : InaBatteryLevel::Empty;
        case InaBatteryLevel::Low:
            if (voltage < (INA226_BAT_EMPTY_MAX_V - h)) return InaBatteryLevel::Empty;
            if (voltage >= (INA226_BAT_LOW_MAX_V + h)) return InaBatteryLevel::Mid;
            return InaBatteryLevel::Low;
        case InaBatteryLevel::Mid:
            if (voltage < (INA226_BAT_LOW_MAX_V - h)) return InaBatteryLevel::Low;
            if (voltage >= (INA226_BAT_MID_MAX_V + h)) return InaBatteryLevel::Full;
            return InaBatteryLevel::Mid;
        case InaBatteryLevel::Full:
        default:
            return (voltage < (INA226_BAT_MID_MAX_V - h)) ? InaBatteryLevel::Mid
                                                          : InaBatteryLevel::Full;
    }
}

void Ina226Hal::publishLevelIfChanged_(InaBatteryLevel level) {
    if (level == _batteryLevel) {
        return;
    }

    _batteryLevel = level;
    if (_levelChangedCallback) {
        _levelChangedCallback(level);
    }
}

void IRAM_ATTR Ina226Hal::onAlertIsr() {
    _dataReadyFlag = true;
}

void Ina226Hal::clearError_() {
    _lastError[0] = '\0';
}

void Ina226Hal::setError_(const char* message) {
    if (!message) {
        clearError_();
        return;
    }

    strncpy(_lastError, message, ERROR_BUFFER_SIZE - 1);
    _lastError[ERROR_BUFFER_SIZE - 1] = '\0';
}
