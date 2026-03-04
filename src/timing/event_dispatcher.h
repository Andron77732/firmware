#pragma once

#include "config.h"

struct tm;
class MainArea;

void event_dispatcher_handle_event_isr(ModuleType module_type,
                                       MainArea& mainArea);

void sendTimedPacket(char header, const struct tm &tm);
