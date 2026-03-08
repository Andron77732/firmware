#include "ina226.h"

#include "config.h"
#include "esp_log.h"

#include <Wire.h>
#include <cmath>
#include <cstdio>
#include <cstring>

static const char* TAG = "INA226";

namespace {
struct Ina226CadenceConfig {
    uint8_t averageEnum;
    uint8_t conversionTimeEnum;
    uint32_t periodMs;
};

constexpr uint16_t kIna226AverageTable[8] = {1, 4, 16, 64, 128, 256, 512, 1024};
constexpr uint16_t kIna226ConversionTimeUsTable[8] = {140, 204, 332, 588,
                                                      1100, 2100, 4200, 8300};
constexpr uint8_t kIna226BootstrapAverage = INA226_16_SAMPLES;
constexpr uint8_t kIna226BootstrapConversionTime = INA226_588_us;
constexpr uint32_t kIna226BootstrapTimeoutMs = 250UL;

Ina226CadenceConfig selectCadenceConfig(uint32_t targetMs) {
    Ina226CadenceConfig best = {INA226_1_SAMPLE, INA226_1100_us, 2};
    uint32_t bestDiff = 0xFFFFFFFFUL;

    for (uint8_t avgEnum = 0; avgEnum < 8; ++avgEnum) {
        for (uint8_t convEnum = 0; convEnum < 8; ++convEnum) {
            const uint32_t periodUs = 2UL * kIna226ConversionTimeUsTable[convEnum] *
                                      kIna226AverageTable[avgEnum];
            const uint32_t periodMs = (periodUs + 500UL) / 1000UL;
            const uint32_t diff =
                (periodMs > targetMs) ? (periodMs - targetMs) : (targetMs - periodMs);

            if (diff < bestDiff) {
                bestDiff = diff;
                best = {avgEnum, convEnum, periodMs};
            }
        }
    }

    return best;
}
} // namespace

Ina226Hal ina226;
volatile bool Ina226Hal::_dataReadyFlag = false;

Ina226Hal::Ina226Hal() : _sensor(INA226_I2C_ADDRESS, &Wire), _wire(&Wire) {}

bool Ina226Hal::begin() {
    return begin(Wire);
}

bool Ina226Hal::begin(TwoWire& wire) {
    if (_initialized) {
        return true;
    }

    _wire = &wire;
    _sensor = INA226(INA226_I2C_ADDRESS, _wire);

    if (!_sensor.begin()) {
        setError_("INA226 not found on I2C");
        ESP_LOGE(TAG, "%s (SDA:%d, SCL:%d, addr:0x%02X)", _lastError,
                 I2C_SDA_PIN, I2C_SCL_PIN, INA226_I2C_ADDRESS);
        return false;
    }

    const int calibrationError =
        _sensor.setMaxCurrentShunt(INA226_MAX_CURRENT_A, INA226_SHUNT_OHMS);
    if (calibrationError != INA226_ERR_NONE) {
        const float shuntVoltage = INA226_MAX_CURRENT_A * INA226_SHUNT_OHMS;
        snprintf(_lastError, ERROR_BUFFER_SIZE,
                 "Calibration failed, err=%d, R=%.4f, Imax=%.3f, Vshunt=%.4fV",
                 calibrationError, INA226_SHUNT_OHMS, INA226_MAX_CURRENT_A,
                 shuntVoltage);
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    // Bootstrap initial values
    _hasValidSample = false;
    _busVoltage = NAN;
    _current = NAN;
    _power = NAN;
    _batteryLevel = InaBatteryLevel::NoData;
    _batteryPercent = -1;
    _dataReadyFlag = false;

    if (!_sensor.setAverage(kIna226BootstrapAverage)) {
        setError_("Failed to set bootstrap averaging");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setBusVoltageConversionTime(kIna226BootstrapConversionTime)) {
        setError_("Failed to set bootstrap bus conversion time");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setShuntVoltageConversionTime(kIna226BootstrapConversionTime)) {
        setError_("Failed to set bootstrap shunt conversion time");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setModeShuntBusContinuous()) {
        setError_("Failed to set bootstrap continuous mode");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (_sensor.waitConversionReady(kIna226BootstrapTimeoutMs)) {
        if (!readAndPublishSample_()) {
            ESP_LOGW(TAG, "Initial INA226 sample invalid: %s", _lastError);
        }
    } else {
        ESP_LOGW(TAG, "Initial INA226 sample timeout (%lums)",
                 static_cast<unsigned long>(kIna226BootstrapTimeoutMs));
    }

    // Setup data_ready interrupt
    const Ina226CadenceConfig cadence = selectCadenceConfig(INA226_SAMPLE_PERIOD_MS);
    if (!_sensor.setAverage(cadence.averageEnum)) {
        setError_("Failed to set averaging");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setBusVoltageConversionTime(cadence.conversionTimeEnum)) {
        setError_("Failed to set bus conversion time");
        ESP_LOGE(TAG, "%s", _lastError);
        return false;
    }

    if (!_sensor.setShuntVoltageConversionTime(cadence.conversionTimeEnum)) {
        setError_("Failed to set shunt conversion time");
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

    attachInterrupt(irq, Ina226Hal::onAlertIsr, FALLING);

    clearError_();

    ESP_LOGI(TAG,
             "Initialized (SDA:%d, SCL:%d, addr:0x%02X, alert:%d, shunt=%.6f, "
             "maxCurrent=%.3f, targetPeriod=%lums, actualPeriod=%lums)",
             I2C_SDA_PIN, I2C_SCL_PIN, INA226_I2C_ADDRESS, INA226_ALERT_PIN,
             INA226_SHUNT_OHMS, INA226_MAX_CURRENT_A,
             static_cast<unsigned long>(INA226_SAMPLE_PERIOD_MS),
             static_cast<unsigned long>(cadence.periodMs));
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

    readAndPublishSample_();
}

bool Ina226Hal::readAndPublishSample_() {
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
        publishLevelIfChanged_(InaBatteryLevel::NoData, -1);
        ESP_LOGW(TAG, "%s", _lastError);
        return false;
    }

    _busVoltage = busVoltage;
    _current = current;
    _power = power;
    _hasValidSample = true;
    clearError_();

    const InaBatteryLevel nextLevel = applyHysteresis_(busVoltage);
    const int percent = percentFromVoltage_(busVoltage);
    publishLevelIfChanged_(nextLevel, percent);
    return true;
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

int Ina226Hal::percentFromVoltage_(float voltage) const {
    if (!std::isfinite(voltage)) {
        return -1;
    }

    // Линейная оценка для Li-ion 2S: 6.8V -> 0%, 8.4V -> 100%.
    static constexpr float kPercentMinV = INA226_BAT_EMPTY_MAX_V;
    static constexpr float kPercentMaxV = 8.4f;

    if (voltage <= kPercentMinV) {
        return 0;
    }
    if (voltage >= kPercentMaxV) {
        return 100;
    }

    const float normalized = (voltage - kPercentMinV) / (kPercentMaxV - kPercentMinV);
    const int percent = static_cast<int>(lroundf(normalized * 100.0f));
    if (percent < 0) return 0;
    if (percent > 100) return 100;
    return percent;
}

void Ina226Hal::publishLevelIfChanged_(InaBatteryLevel level, int percent) {
    if (level == _batteryLevel && percent == _batteryPercent) {
        return;
    }

    _batteryLevel = level;
    _batteryPercent = percent;
    if (_levelChangedCallback) {
        _levelChangedCallback(level, percent);
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
