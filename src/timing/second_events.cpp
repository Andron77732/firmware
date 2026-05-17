#include "second_events.h"
#include "hal/ble/ble.h"
#include "storage/settings.h"
#include "timing/event_dispatcher.h"
#include "ui/status_bar.h"
#include "ui/main_area.h"
#include <Arduino.h>
#include <time.h>

void second_events_handle_tick(ModuleType module_type, StatusBar& statusBar,
                               MainArea& mainArea) {
  static time_t lastTimeSec = 0;

  time_t nowSec = time(nullptr);

  if (nowSec <= 0)
    return; // Время ещё не установлено

  if (nowSec != lastTimeSec) {
    lastTimeSec = nowSec;

    int16_t timezone_offset_min = settings.getDevice().timezone_offset_min;
    time_t localSec = nowSec + (time_t)timezone_offset_min * 60;
    struct tm tm{};
    gmtime_r(&localSec, &tm);

    // Обновление времени на статус-баре
    statusBar.updateTime(tm);

    if (module_type != ModuleType::START)
      return;

    mainArea.updateCountdown(tm.tm_sec);

    switch (tm.tm_sec) {
    case 56:
      sendTimedPacket(BEEP_HEADER, tm);
      break;
    case 15:
      sendTimedPacket(VOICE_HEADER, tm);
      break;
    default:
      break;
    }
  }
}
