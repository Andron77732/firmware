#ifndef HAL_INA226_H
#define HAL_INA226_H

#include <Arduino.h>
#include <INA226.h>
#include <Wire.h>

enum class InaBatteryLevel : uint8_t {
    NoData = 0,
    Critical = 1,
    Empty = 2,
    Low = 3,
    Mid = 4,
    Full = 5,
};

using InaLevelChangedCallback = void (*)(InaBatteryLevel level, int percent);

class Ina226Hal {
public:
    Ina226Hal();

    bool begin();
    bool begin(TwoWire& wire);
    void update();

    bool isReady() const { return _initialized; }
    bool hasValidSample() const { return _hasValidSample; }

    float getBusVoltage() const { return _busVoltage; }  // V
    float getCurrent() const { return _current; }        // A
    float getPower() const { return _power; }            // W
    InaBatteryLevel batteryLevel() const { return _batteryLevel; }
    int batteryPercent() const { return _batteryPercent; }  // -1 = no data

    const char* lastError() const { return _lastError; }
    void setLevelChangedCallback(InaLevelChangedCallback callback);

    static void IRAM_ATTR onAlertIsr();

private:
    INA226 _sensor;
    TwoWire* _wire = nullptr;
    bool _initialized = false;
    bool _hasValidSample = false;

    float _busVoltage = NAN;
    float _current = NAN;
    float _power = NAN;
    volatile InaBatteryLevel _batteryLevel = InaBatteryLevel::NoData;
    int _batteryPercent = -1;
    static volatile bool _dataReadyFlag;

    InaLevelChangedCallback _levelChangedCallback = nullptr;

    static constexpr size_t ERROR_BUFFER_SIZE = 96;
    char _lastError[ERROR_BUFFER_SIZE] = {0};

    void processDataReady_();
    bool readAndPublishSample_();
    InaBatteryLevel levelFromVoltage_(float voltage) const;
    InaBatteryLevel applyHysteresis_(float voltage) const;
    int percentFromVoltage_(float voltage) const;
    void publishLevelIfChanged_(InaBatteryLevel level, int percent);

    void clearError_();
    void setError_(const char* message);
};

extern Ina226Hal ina226;

#endif // HAL_INA226_H
