#include "touch_action_handler.h"

#include "esp_log.h"

const char* TAG = "TouchActionHandler";

void TouchActionHandler::init(MainArea& mainArea,
                              SettingsManager& settingsManager,
                              WiFiManager& wifiManager) {
  _mainArea = &mainArea;
  _settings = &settingsManager;
  _wifiManager = &wifiManager;
}

void TouchActionHandler::handleTouchEvent(const TouchEvent& event,
                                          UiTouchTarget target) {
  if (!_mainArea || !_settings || !_wifiManager ||
      event.type != TouchEventType::Release) {
    return;
  }

  if (target == UiTouchTarget::StatusBarWifi) {
    handleWiFiToggle_();
    return;
  }

  if (target == UiTouchTarget::StatusBarBackground) {
    handleStatusBarBackgroundTap_();
  }
}

MainAreaType TouchActionHandler::resolveWorkingMainAreaType_() const {
  const DeviceSettings& device = _settings->getDevice();
  if (device.type == 2) {
    return MainAreaType::FINISH;
  }

  return MainAreaType::START;
}

void TouchActionHandler::handleStatusBarBackgroundTap_() {
  const MainAreaType currentType = _mainArea->getType();
  const MainAreaType workingType = resolveWorkingMainAreaType_();
  const MainAreaType nextType =
      currentType == MainAreaType::LOADING ? workingType : MainAreaType::LOADING;

  _mainArea->setType(nextType);
  _mainArea->draw();
}

void TouchActionHandler::handleWiFiToggle_() {
  switch (_wifiManager->getState()) {
  case WiFiState::UNINITIALIZED:
  case WiFiState::OFF: {
    const WifiSettings& wifi = _settings->getWifi();
    if (wifi.ssid.length() == 0) {
      ESP_LOGW(TAG, "WiFi enable requested from touch, but SSID is not configured");
      _mainArea->addLogLine("ERROR: WiFi is not configured");
      return;
    }

    _wifiManager->begin();
    if (!_wifiManager->connect(wifi.ssid, wifi.passwd)) {
      ESP_LOGE(TAG, "WiFi enable requested from touch failed");
      _mainArea->addLogLine("ERROR: WiFi connect failed");
      return;
    }

    ESP_LOGI(TAG, "WiFi enable requested from touch: ssid=%s", wifi.ssid.c_str());
    _mainArea->addLogLine("WiFi connecting...");
    return;
  }
  case WiFiState::CONNECTING:
  case WiFiState::CONNECTED:
  case WiFiState::DISCONNECTED:
  case WiFiState::RECONNECTING:
  case WiFiState::ERROR:
    if (!_wifiManager->end()) {
      ESP_LOGW(TAG, "WiFi disable requested from touch timed out");
      _mainArea->addLogLine("ERROR: WiFi stop timeout");
      return;
    }

    ESP_LOGI(TAG, "WiFi disabled from touch");
    _mainArea->addLogLine("WiFi disabled");
    return;
  }
}
