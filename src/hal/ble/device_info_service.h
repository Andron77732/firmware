#pragma once
#include "ble.h"  // IBleServicePlugin
#include "config.h"
#include <stdint.h>

class BleDeviceInfoService : public IBleServicePlugin {
public:
  bool init(NimBLEServer* server) override;

private:
  const char* _manufacturer = BLE_DIS_MANUFACTURER;
  const char* _model        = BLE_DIS_MODEL;
  const char* _serial       = nullptr;      // если nullptr/"" -> автогенерация
  const char* _hw           = BLE_DIS_HARDWARE;
  const char* _fw           = VERSION;
  const char* _sw           = BLE_DIS_SOFTWARE;

  const char* _serialPrefix   = BLE_DIS_SERIAL_PREFIX;
  char _serialBuf[32]       = {0};          // статический буфер для авто-serial

  NimBLEService* _service = nullptr;

private:
  void buildAutoSerialIfNeeded();
};

extern BleDeviceInfoService deviceInfoService;
