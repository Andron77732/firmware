#include "touch_calibration_service.h"

#include "hal/tft/tft.h"
#include "hal/touch/touch.h"
#include "storage/settings.h"
#include "ui/touch_calibration_screen.h"

static bool s_uiRedrawPending = false;

TouchCalibrationFlowResult runTouchCalibrationFlow(bool interactiveUi) {
  TouchCalibrationFlowResult result;

  if (!touch.isReady()) {
    result.status = TouchCalibrationFlowStatus::TouchNotReady;
    return result;
  }

  TouchCalibration calibration;
  bool calibrated = false;
  if (interactiveUi) {
    calibrated = runTouchCalibrationScreen(display.tft(), touch, calibration);
    s_uiRedrawPending = true;
  } else {
    calibrated = touch.calibrate(calibration);
  }

  if (!calibrated) {
    result.status = TouchCalibrationFlowStatus::WizardFailed;
    return result;
  }

  TouchSettings touchSettings = settings.getTouch();
  touchSettings.calibration = calibration;
  if (!settings.setTouch(touchSettings)) {
    result.status = TouchCalibrationFlowStatus::InvalidSettings;
    return result;
  }

  int savedKeys = settings.save();
  if (savedKeys < 0) {
    result.status = TouchCalibrationFlowStatus::SaveFailed;
    return result;
  }

  if (!touch.applyCalibration(touchSettings.calibration)) {
    result.status = TouchCalibrationFlowStatus::ApplyFailed;
    return result;
  }

  result.status = TouchCalibrationFlowStatus::Ok;
  result.savedKeys = savedKeys;
  result.calibration = touchSettings.calibration;
  return result;
}

bool consumeTouchCalibrationUiRedrawRequest() {
  bool result = s_uiRedrawPending;
  s_uiRedrawPending = false;
  return result;
}
