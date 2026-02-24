#ifndef APP_TOUCH_CALIBRATION_SERVICE_H
#define APP_TOUCH_CALIBRATION_SERVICE_H

#include <Arduino.h>

#include "storage/settings.h"

enum class TouchCalibrationFlowStatus : uint8_t {
  Ok = 0,
  TouchNotReady = 1,
  WizardFailed = 2,
  InvalidSettings = 3,
  SaveFailed = 4,
  ApplyFailed = 5,
};

struct TouchCalibrationFlowResult {
  TouchCalibrationFlowStatus status = TouchCalibrationFlowStatus::TouchNotReady;
  int savedKeys = 0;
  TouchCalibration calibration;
};

TouchCalibrationFlowResult runTouchCalibrationFlow(bool interactiveUi);
bool consumeTouchCalibrationUiRedrawRequest();

#endif // APP_TOUCH_CALIBRATION_SERVICE_H
