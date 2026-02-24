#ifndef UI_TOUCH_CALIBRATION_SCREEN_H
#define UI_TOUCH_CALIBRATION_SCREEN_H

#include <TFT_eSPI.h>

#include "hal/touch/touch.h"

bool runTouchCalibrationScreen(TFT_eSPI& tft, Touch& touch,
                               TouchCalibration& outCalibration);

#endif // UI_TOUCH_CALIBRATION_SCREEN_H
