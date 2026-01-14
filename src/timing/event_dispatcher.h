#pragma once

#include "config.h"

struct tm;

void event_dispatcher_handle_event_isr(ModuleType module_type);

void sendTimedPacket(char header, const struct tm &tm);
