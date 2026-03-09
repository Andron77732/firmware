#ifndef UI_TOUCH_TARGET_H
#define UI_TOUCH_TARGET_H

#include <stdint.h>

enum class UiTouchTarget : uint8_t {
  None = 0,
  StatusBarClock = 1,
  StatusBarGps = 2,
  StatusBarBluetooth = 3,
  StatusBarWifi = 4,
  StatusBarBattery = 5,
  StatusBarBackground = 6,
  MainArea = 7,
  Footer = 8,
};

static const char *uiTouchTargetText(UiTouchTarget target) {
  switch (target) {
  case UiTouchTarget::StatusBarClock:
    return "status_bar_clock";
  case UiTouchTarget::StatusBarGps:
    return "status_bar_gps";
  case UiTouchTarget::StatusBarBluetooth:
    return "status_bar_bluetooth";
  case UiTouchTarget::StatusBarWifi:
    return "status_bar_wifi";
  case UiTouchTarget::StatusBarBattery:
    return "status_bar_battery";
  case UiTouchTarget::StatusBarBackground:
    return "status_bar_background";
  case UiTouchTarget::MainArea:
    return "main_area";
  case UiTouchTarget::Footer:
    return "footer";
  case UiTouchTarget::None:
  default:
    return "unhandled";
  }
}

#endif // UI_TOUCH_TARGET_H
