#include "command_router.h"
#include "handlers.h"
#include "esp_log.h"
#include <ArduinoJson.h>
#include <string.h>

static const char *TAG = "CommandRouter";

// Таблица маршрутизации команд
static const CommandRoute commandRoutes[] = {
    {"ping", cmdPing},
    // Добавьте здесь новые команды:
    // {"setNumber", cmdSetNumber},
};

static const size_t commandRoutesCount = sizeof(commandRoutes) / sizeof(commandRoutes[0]);

void CommandRouter::route(JsonDocument& doc, Stream& output) {
    // Проверяем наличие поля cmd

    if (doc["cmd"].isNull() || !doc["cmd"].is<const char*>()) {
        ESP_LOGW(TAG, "Command missing 'cmd' field or invalid type");
        JsonDocument errorResponse;
        errorResponse["status"] = "error";
        errorResponse["error_code"] = 101;
        errorResponse["error_message"] = "Missing or invalid 'cmd' field";
        sendResponse(errorResponse, output, true);
        return;
    }
    
    const char* cmd = doc["cmd"].as<const char*>();
    JsonVariant requestId;
    if (!doc["id"].isNull()) {
        requestId = doc["id"];
    }
    
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
        sendError(cmd, 100, "Unknown command", requestId, output);
    }
}

void CommandRouter::sendError(const char* cmd, int errorCode, const char* errorMessage, 
                              const JsonVariant& requestId, Stream& output) {
    JsonDocument response;
    
    response["cmd"] = cmd;
    response["status"] = "error";
    response["error_code"] = errorCode;
    response["error_message"] = errorMessage;
    
    // Копируем id из запроса, если оно есть
    if (!requestId.isNull()) {
        response["id"] = requestId;
    }
    
    sendResponse(response, output, true);
}
