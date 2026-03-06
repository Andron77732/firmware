#ifndef HAL_INA226_H
#define HAL_INA226_H

#include <Arduino.h>
#include <INA226.h>

class Ina226Hal {
public:
    Ina226Hal();

    bool begin();
    void update();

    bool isReady() const { return _initialized; }
    bool hasValidSample() const { return _hasValidSample; }
    bool hasNewSample();

    float getBusVoltage() const { return _busVoltage; }  // V
    float getCurrent() const { return _current; }        // A
    float getPower() const { return _power; }            // W

    const char* lastError() const { return _lastError; }

    static void IRAM_ATTR onAlertIsr();

private:
    INA226 _sensor;
    bool _initialized = false;
    bool _hasValidSample = false;
    bool _hasNewSample = false;

    float _busVoltage = NAN;
    float _current = NAN;
    float _power = NAN;

    static volatile bool _dataReadyFlag;

    static constexpr size_t ERROR_BUFFER_SIZE = 96;
    char _lastError[ERROR_BUFFER_SIZE] = {0};

    void clearError_();
    void setError_(const char* message);
};

extern Ina226Hal ina226;

#endif // HAL_INA226_H
