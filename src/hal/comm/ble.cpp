// ble.cpp
#include "ble.h"
#include "esp_log.h"
#include <algorithm> 

static const char *TAG = "BLE";
BLESerial bleSerial;

// ============================================================================
// Callbacks
// ============================================================================

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    uint16_t mtu = connInfo.getMTU();
    ESP_LOGI(TAG, "Client connected: %s, MTU: %d",
             connInfo.getAddress().toString().c_str(), mtu);

    bleSerial.onConnect(mtu);

    // Настройка connection interval для низкой latency (быстрое соединение)
    // min_interval: 6 = 7.5ms, max_interval: 6 = 7.5ms
    // latency: 0 = без пропусков, timeout: 500 = 5 секунд
    pServer->updateConnParams(connInfo.getConnHandle(), 6, 6, 0, 500);
    ESP_LOGI(
        TAG,
        "Conn params requested: min=7.5ms, max=15ms, latency=0, timeout=5s");
  }

  void onDisconnect(NimBLEServer * /*pServer*/, NimBLEConnInfo & /*connInfo*/,
                    int reason) override {
    ESP_LOGI(TAG, "Client disconnected, reason: %d", reason);

    bleSerial.onDisconnect();

    // Только старт рекламы — payload уже настроен в begin()
    bleSerial.startAdvertising();
  }

  void onMTUChange(uint16_t mtu, NimBLEConnInfo & /*connInfo*/) override {
    ESP_LOGI(TAG, "MTU updated: %d", mtu);
    bleSerial.onMtuUpdated(mtu);
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic,
               NimBLEConnInfo & /*connInfo*/) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      bleSerial.onReceive(reinterpret_cast<const uint8_t *>(value.data()),
                          value.size());
    }
  }
};

class TxCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic * /*pCharacteristic*/,
                   NimBLEConnInfo & /*connInfo*/, uint16_t subValue) override {
    bool notify = (subValue & 0x01) != 0;
    ESP_LOGI(TAG, "Notify %s", notify ? "enabled" : "disabled");
    bleSerial.onNotifyStateChanged(notify);
  }
};

// ============================================================================
// BLESerial Implementation
// ============================================================================

void BLESerial::init(const String &deviceName) {
  if (_server) {
    ESP_LOGW(TAG, "BLE already initialized");
    return;
  }

  ESP_LOGI(TAG, "Initializing NUS as '%s'...", deviceName.c_str());
  _deviceName = deviceName;

  _connected = false;
  _notifyEnabled = false;
  _mtu = 23;
  _hasPeeked = false;
  _peekedByte = -1;
  _advConfigured = false;
  
  // Создание StreamBuffer для RX (thread-safe)
  _rxStream = xStreamBufferCreate(BLE_RX_BUFFER_SIZE, 1);
  if (!_rxStream) {
    ESP_LOGE(TAG, "Failed to create RX StreamBuffer (%d bytes)",
             BLE_RX_BUFFER_SIZE);
    return;
  }
  ESP_LOGI(TAG, "RX StreamBuffer created: %d bytes", BLE_RX_BUFFER_SIZE);

  // Инициализация BLE устройства
  NimBLEDevice::init(deviceName.c_str());
  // Установка MTU для максимального размера пакетов
  NimBLEDevice::setMTU(517);

  // Создание сервера BLE
  _server = NimBLEDevice::createServer();
  _server->setCallbacks(new ServerCallbacks());

  // Создание сервиса NUS
  _service = _server->createService(NUS_SERVICE_UUID);

  // RX характеристика (клиент пишет сюда)
  _rxCharacteristic = _service->createCharacteristic(
      NUS_RX_CHARACTERISTIC,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  _rxCharacteristic->setCallbacks(new RxCallbacks());

  // TX характеристика (ESP отправляет через notify)
  _txCharacteristic = _service->createCharacteristic(NUS_TX_CHARACTERISTIC,
                                                     NIMBLE_PROPERTY::NOTIFY);
  _txCharacteristic->setCallbacks(new TxCallbacks());

  // Запуск сервиса
  _service->start();

  ESP_LOGI(TAG, "NUS initialized");
}

void BLESerial::setupAdvertisingOnce() {
  if (_advConfigured)
    return;

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  if (!adv) {
    ESP_LOGW(TAG, "Cannot configure advertising: NimBLEAdvertising is null");
    return;
  }

  // Полный сброс один раз
  adv->stop();
  adv->reset();

  // ---------- Advertisement packet ----------
  // Минимальный: только UUID сервиса
  NimBLEAdvertisementData advData;
  advData.addServiceUUID(NUS_SERVICE_UUID);
  adv->setAdvertisementData(advData);

  // ---------- Scan Response packet ----------
  // Имя устройства уходит сюда
  NimBLEAdvertisementData scanData;
  scanData.setName(_deviceName.c_str());
  adv->setScanResponseData(scanData);

  _advConfigured = true;
  ESP_LOGI(TAG, "Advertising payload configured once (ADV + scan response)");
}

void BLESerial::startAdvertising() {
  if (!_server) {
    ESP_LOGW(TAG, "Cannot start advertising: server not initialized");
    return;
  }

  if (!_advConfigured) {
    setupAdvertisingOnce();
    if (!_advConfigured)
      return; // не смогли сконфигурировать
  }

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  if (!adv) {
    ESP_LOGW(TAG, "Cannot start advertising: NimBLEAdvertising is null");
    return;
  }

  if (adv->isAdvertising()) {
    notifyStateChanged();
    return;
  }

  adv->start();
  ESP_LOGI(TAG, "Advertising started");
  notifyStateChanged();
}

void BLESerial::stopAdvertising() {
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  if (adv && adv->isAdvertising()) {
    adv->stop();
    ESP_LOGI(TAG, "Advertising stopped");
    notifyStateChanged();
  }
}

void BLESerial::end() {
  NimBLEDevice::deinit(true);

  _server = nullptr;
  _service = nullptr;
  _rxCharacteristic = nullptr;
  _txCharacteristic = nullptr;

  _connected = false;
  _notifyEnabled = false;
  _advConfigured = false;

  // Освобождение StreamBuffer
  if (_rxStream) {
    vStreamBufferDelete(_rxStream);
    _rxStream = nullptr;
  }

  _hasPeeked = false;
  _peekedByte = -1;

  notifyStateChanged();
}

bool BLESerial::isConnected() { return _connected; }

bool BLESerial::isAdvertising() {
  if (!_server)
    return false;
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  return adv ? adv->isAdvertising() : false;
}

BLEState BLESerial::getState() {
  if (_connected)
    return BLEState::CONNECTED;
  if (isAdvertising())
    return BLEState::ADVERTISING;
  return BLEState::DISCONNECTED;
}

void BLESerial::setStateCallback(BLEStateCallback callback) {
  _stateCallback = callback;
  notifyStateChanged();
}

// ============================================================================
// Stream Interface
// ============================================================================

int BLESerial::available() {
  if (!_rxStream)
    return 0;
  size_t avail = xStreamBufferBytesAvailable(_rxStream);
  return static_cast<int>(avail + (_hasPeeked ? 1 : 0));
}

int BLESerial::read() {
  // Если есть peeked байт, вернуть его
  if (_hasPeeked) {
    _hasPeeked = false;
    return _peekedByte;
  }
  if (!_rxStream)
    return -1;

  uint8_t c;
  if (xStreamBufferReceive(_rxStream, &c, 1, 0) == 1)
    return c;
  return -1;
}

int BLESerial::peek() {
  // Если есть peeked байт, вернуть его
  if (_hasPeeked)
    return _peekedByte;
  if (!_rxStream)
    return -1;

  // Прочитать байт и сохранить для следующего read()
  uint8_t c;
  if (xStreamBufferReceive(_rxStream, &c, 1, 0) == 1) {
    _peekedByte = c;
    _hasPeeked = true;
    return c;
  }
  return -1;
}

void BLESerial::flush() {
  // noop
  // Для TX — ничего не делаем, notify отправляется сразу
}

size_t BLESerial::write(uint8_t c) { return write(&c, 1); }

size_t BLESerial::write(const uint8_t *buffer, size_t size) {
  if (!_connected || !_txCharacteristic || !_notifyEnabled || !buffer ||
      size == 0) {
    return 0;
  }

  // Используем согласованный MTU минус 3 байта ATT header
  const size_t maxChunk = (_mtu > 3) ? (_mtu - 3) : 20;
  const int maxRetries = 50;
  size_t sent = 0;

  while (sent < size) {
    size_t chunk = min(maxChunk, size - sent);
    _txCharacteristic->setValue(buffer + sent, chunk);

    // Retry loop для congestion control
    int retries = 0;
    while (!_txCharacteristic->notify()) {
      retries++;
      if (retries >= maxRetries) {
        ESP_LOGW(TAG, "Notify failed after %d retries, sent %u/%u bytes",
                 maxRetries, (unsigned)sent, (unsigned)size);
        return sent;
      }
      // Адаптивный backoff:
      // первые попытки — агрессивно, дальше мягче
      if (retries < 20) {
        vTaskDelay(pdMS_TO_TICKS(1));
      } else if (retries < 30) {
        vTaskDelay(pdMS_TO_TICKS(2));
      } else if (retries < 40) {
        vTaskDelay(pdMS_TO_TICKS(4));
      } else {
        vTaskDelay(pdMS_TO_TICKS(8));
      }

      // Проверка что всё ещё подключены
      if (!_connected || !_notifyEnabled)
        return sent;
    }

    sent += chunk;
  }

  return sent;
}

// ============================================================================
// Internal Callbacks
// ============================================================================

void BLESerial::onReceive(const uint8_t *data, size_t len) {
  if (!_rxStream || !data || len == 0)
    return;

  size_t pushed = xStreamBufferSend(_rxStream, data, len, 0);
  if (pushed < len) {
    // Rate limit overflow warnings (max once per second)
    static uint32_t lastOverflowLog = 0;
    static size_t overflowEvents = 0;

    overflowEvents++;
    uint32_t now = xTaskGetTickCount();

    if (now - lastOverflowLog > pdMS_TO_TICKS(1000)) {
      ESP_LOGW(TAG, "RX overflow: lost %u bytes (%u events)",
               (unsigned)(len - pushed), (unsigned)overflowEvents);
      lastOverflowLog = now;
      overflowEvents = 0;
    }
  }
}

void BLESerial::onConnect(uint16_t mtu) {
  _connected = true;
  _mtu = mtu;
  notifyStateChanged();
}

void BLESerial::onDisconnect() {
  _connected = false;
  _notifyEnabled = false;
  notifyStateChanged();
}

void BLESerial::onMtuUpdated(uint16_t mtu) { _mtu = mtu; }

void BLESerial::onNotifyStateChanged(bool enabled) { _notifyEnabled = enabled; }

void BLESerial::notifyStateChanged() {
  // Уведомляем об изменении состояния с текущим состоянием
  if (_stateCallback)
    _stateCallback(getState());
}
