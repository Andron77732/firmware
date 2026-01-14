#include "event_dispatcher.h"
#include "hal/comm/ble.h"
#include "storage/settings.h"
#include "timing/event_isr.h"
#include "timing/event_timestamp.h"
#include "ui/main_area.h"
#include <Arduino.h>
#include <time.h>

static void sendTimedPacket(char header, const struct tm &tm) {
  char timebuf[9];
  String packet;
  snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min,
           tm.tm_sec);
  packet.reserve(11);
  packet += header;
  packet += timebuf;
  packet += PACKET_ENDER;
  packet += '\n';
  Serial.print(packet);
  bleSerial.print(packet);
}

static void sendEventPacket(const EventTimestampData &data) {
  if (!data.success)
    return;

  char packet[40];
  int len = 0;

  if (data.module_type == ModuleType::START) {
    int32_t correction_ms = data.correction_ms;
    int32_t abs_corr = (correction_ms < 0) ? -correction_ms : correction_ms;
    if (abs_corr > (int32_t)MAX_CORRECTION_MS) {
      len = snprintf(packet, sizeof(packet), "%c%s%c\n", START_HEADER,
                     data.local_time_str, PACKET_ENDER);
    } else {
      len = snprintf(packet, sizeof(packet), "%c%s;%ld%c\n", START_HEADER,
                     data.local_time_str, (long)correction_ms, PACKET_ENDER);
    }
  } else {
    len = snprintf(packet, sizeof(packet), "%c%s%c\n", FINISH_HEADER,
                   data.local_time_str, PACKET_ENDER);
  }

  if (len <= 0)
    return;

  size_t send_len =
      (len < (int)sizeof(packet)) ? (size_t)len : (sizeof(packet) - 1);
  Serial.write(packet, send_len);
  bleSerial.write((const uint8_t *)packet, send_len);
}

void event_dispatcher_update_second_events(ModuleType module_type) {
  static time_t lastTimeSec = 0;

  if (module_type != ModuleType::START)
    return;

  time_t nowSec = time(nullptr);

  if (nowSec <= 0)
    return; // Время ещё не установлено

  if (nowSec != lastTimeSec) {
    lastTimeSec = nowSec;

    int8_t timezone = settings.getDevice().timezone;
    time_t localSec = nowSec + (time_t)timezone * 3600;
    struct tm tm{};
    gmtime_r(&localSec, &tm);

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

void event_dispatcher_handle_event_isr(ModuleType module_type) {
  int64_t t_esp_us = 0;

  if (event_isr_get(t_esp_us)) {
    EventTimestampData data = event_timestamp_process(t_esp_us, module_type);
    sendEventPacket(data);
    mainArea.displayEventTimestamp(data);
  }
}
