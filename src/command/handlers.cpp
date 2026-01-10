#include "handlers.h"
#include "storage/settings.h"
#include "timing/time_sync.h"
#include "esp_log.h"
#include <ArduinoJson.h>
#include <esp_timer.h>

static const char *TAG = "CommandHandlers";

void cmdPing(JsonDocument& request, Stream& output) {
    JsonDocument response;
    
    // Копируем поле id из запроса, если оно есть
    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }
    
    // Формируем ответ
    response["cmd"] = "pong";
    response["status"] = "ok";
    
    sendResponse(response, output, true);
    
    ESP_LOGI(TAG, "Ping command processed, sent pong response");
}

void cmdTime(JsonDocument& request, Stream& output) {
    JsonDocument response;
    
    // Копируем поле id из запроса, если оно есть
    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }
    
    response["cmd"] = "time";
    
    // Получаем текущее время ESP таймера
    int64_t esp_time_us = esp_timer_get_time();
    
    // Конвертируем в UTC микросекунды
    int64_t utc_time_us = 0;
    bool time_valid = time_sync_esp_to_utc_us(esp_time_us, utc_time_us);
    
    // Получаем статус синхронизации
    TimeSyncStatus sync_status = time_sync_status();
    
    if (!time_valid || sync_status.source == TimeSource::NONE) {
        // Нет источника времени
        const char* cmd = request["cmd"].as<const char*>();
        sendError(cmd, 201, "RTC not initialized or no time source available", request["id"], output);
        ESP_LOGW(TAG, "Time command failed: no time source available");
        return;
    }
    
    // Определяем источник времени и точность
    const char* source_str = "rtc";
    int64_t accuracy_us = time_sync_estimate_accuracy_us();
    
    // Определяем статус ответа
    const char* status_str = "ok";
    if (sync_status.source == TimeSource::RTC && !sync_status.gps_time_valid) {
        // RTC используется, но GPS был потерян
        status_str = "warning";
    }
    
    // Формируем ответ
    response["time"] = utc_time_us;
    response["source"] = source_str;
    response["accuracy_us"] = accuracy_us;
    response["status"] = status_str;
    
    sendResponse(response, output, true);
    
    ESP_LOGI(TAG, "Time command processed: time=%lld us, source=%s, accuracy=%lld us, status=%s",
             (long long)utc_time_us, source_str, (long long)accuracy_us, status_str);
}

void cmdLoadConfig(JsonDocument& request, Stream& output) {
    JsonDocument response;

    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }

    response["cmd"] = "load_config";
    response["status"] = "ok";

    JsonDocument settings_doc = settings.toJson();
    response["data"] = settings_doc.as<JsonVariantConst>();

    sendResponse(response, output, true);

    ESP_LOGI(TAG, "Load_config command processed");
}

void cmdSaveConfig(JsonDocument& request, Stream& output) {
    if (request["data"].isNull()) {
        sendError("save_config", 102, "Missing 'data' field", request["id"], output);
        ESP_LOGW(TAG, "Save_config failed: missing data");
        return;
    }

    if (!request["data"].is<JsonObject>()) {
        sendError("save_config", 103, "Invalid 'data' field type", request["id"], output);
        ESP_LOGW(TAG, "Save_config failed: invalid data type");
        return;
    }

    JsonDocument data_doc;
    data_doc.set(request["data"]);

    if (!settings.fromJson(data_doc)) {
        sendError("save_config", 103, "Invalid config values", request["id"], output);
        ESP_LOGW(TAG, "Save_config failed: validation error");
        return;
    }

    int saved_keys = settings.save();
    if (saved_keys < 0) {
        sendError("save_config", 202, "Failed to save settings", request["id"], output);
        ESP_LOGW(TAG, "Save_config failed: storage error");
        return;
    }

    JsonDocument response;

    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }

    response["cmd"] = "save_config";
    response["saved_keys"] = saved_keys;
    response["status"] = "ok";

    size_t used_bytes = 0;
    size_t total_bytes = 0;
    if (settings.getStorageStats(used_bytes, total_bytes) && total_bytes > 0) {
        response["storage_usage_percent"] =
            static_cast<int>((used_bytes * 100U) / total_bytes);
    }

    sendResponse(response, output, true);

    ESP_LOGI(TAG, "Save_config command processed: saved_keys=%d", saved_keys);
}

void sendResponse(JsonDocument& response, Stream& output, bool addNewline) {
    String jsonStr;
    serializeJson(response, jsonStr);
    if (addNewline) {
        jsonStr += '\n';
    }
    output.write((const uint8_t*)jsonStr.c_str(), jsonStr.length());
}

void sendError(const char* cmd, int errorCode, const char* errorMessage, 
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
