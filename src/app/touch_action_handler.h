#ifndef APP_TOUCH_ACTION_HANDLER_H
#define APP_TOUCH_ACTION_HANDLER_H

#include "hal/touch/touch.h"
#include "hal/wifi/wifi.h"
#include "storage/settings.h"
#include "ui/main_area.h"
#include "ui/ui_touch_target.h"

class TouchActionHandler {
public:
  void init(MainArea& mainArea,
            SettingsManager& settingsManager,
            WiFiManager& wifiManager);
  void handleTouchEvent(const TouchEvent& event, UiTouchTarget target);

private:
  MainArea* _mainArea = nullptr;
  SettingsManager* _settings = nullptr;
  WiFiManager* _wifiManager = nullptr;

  MainAreaType resolveWorkingMainAreaType_() const;
  void handleBatteryTap_();
  void handleGpsTap_();
  void handleStatusBarBackgroundTap_();
  void handleWiFiToggle_();
};

#endif // APP_TOUCH_ACTION_HANDLER_H
