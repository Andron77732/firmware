#include "touch_calibration_screen.h"

#include <Arduino.h>

bool runTouchCalibrationScreen(TFT_eSPI& tft, Touch& touch,
                               TouchCalibration& outCalibration) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print("Touch calibration");
  tft.setTextSize(1);
  tft.setCursor(8, 32);
  tft.print("Tap corners as requested");

  bool ok = touch.calibrate(outCalibration);

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  if (ok) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("Calibration saved");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(8, 32);
    tft.print("Applying touch settings...");
    delay(700);
    return true;
  }

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.print("Calibration failed");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(8, 32);
  tft.print("Retrying...");
  delay(800);
  return false;
}
