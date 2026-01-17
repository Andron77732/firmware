#include "command_router.h"
#include "handlers.h"
#include "esp_log.h"
#include <ArduinoJson.h>
#include <string.h>

static const char *TAG = "CommandRouter";

// Таблица маршрутизации команд
static const CommandRoute commandRoutes[] = {
    {"ping", cmdPing},
    {"time", cmdTime},
    {"status", cmdStatus},
    {"load_config", cmdLoadConfig},
    {"save_config", cmdSaveConfig},
    {"sync_ntp", cmdSyncNtp},
    {"wifi", cmdWifi},
    {"sync_source", cmdSyncSource},
    {"calibrate", cmdCalibrate},
    {"factory_reset", cmdFactoryReset},
    // Добавьте здесь новые команды:
    // {"setNumber", cmdSetNumber},
};

static const size_t commandRoutesCount = sizeof(commandRoutes) / sizeof(commandRoutes[0]);

void CommandRouter::route(JsonDocument& doc, Stream& output) {
    // Проверяем наличие поля cmd
    if (doc["cmd"].isNull() || !doc["cmd"].is<const char*>()) {
        ESP_LOGW(TAG, "Command missing 'cmd' field or invalid type");
        sendError("", 101, "Missing or invalid 'cmd' field", doc["id"], output);
        return;
    }
    
    const char* cmd = doc["cmd"].as<const char*>();
    
    ESP_LOGI(TAG, "Routing command: %s", cmd);
    
    // Поиск команды в таблице маршрутизации
    bool found = false;
    for (size_t i = 0; i < commandRoutesCount; i++) {
        if (strcmp(cmd, commandRoutes[i].name) == 0) {
            commandRoutes[i].handler(doc, output);
            found = true;
            break;
        }
    }
    
    if (!found) {
        // Неизвестная команда
        ESP_LOGW(TAG, "Unknown command: %s", cmd);
        sendError(cmd, 100, "Unknown command", doc["id"], output);
    }
}
