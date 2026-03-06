#pragma once

#include <NimBLEDevice.h>

#include "ble.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

class BleNusService : public IBleServicePlugin, public Stream {
public:
  bool init(NimBLEServer *server) override;
  void onConnect(NimBLEConnInfo & /*connInfo*/, uint16_t mtu) override;
  void onDisconnect(int /*reason*/) override;
  void onMtuUpdated(uint16_t mtu) override;
  void onBleEnd() override;
  void configureAdvertising(NimBLEAdvertisementData &advData) override;

  bool isConnected() const;

  // Stream
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;

  // Для внутреннего callback-хендлинга.
  void onReceive(const uint8_t *data, size_t len);
  void onNotifyStateChanged(bool enabled);

private:
  NimBLEService *_service = nullptr;
  NimBLECharacteristic *_rxCharacteristic = nullptr;
  NimBLECharacteristic *_txCharacteristic = nullptr;

  StreamBufferHandle_t _rxStream = nullptr;
  int _peekedByte = -1;
  bool _hasPeeked = false;

  bool _connected = false;
  bool _notifyEnabled = false;
  uint16_t _mtu = 23;
};

extern BleNusService nusService;
