#include "handlers.h"
#include "timing/sntp.h"
#include "storage/settings.h"
#include "hal/comm/wifi.h"
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

    Settings prev_settings = settings.getAll();

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

    Settings new_settings = settings.getAll();
    bool reboot_needed =
        prev_settings.device.name != new_settings.device.name ||
        prev_settings.device.number != new_settings.device.number ||
        prev_settings.device.type != new_settings.device.type ||
        prev_settings.device.timezone != new_settings.device.timezone ||
        prev_settings.sync.auto_sync != new_settings.sync.auto_sync ||
        prev_settings.sync.source != new_settings.sync.source ||
        prev_settings.wifi.active != new_settings.wifi.active ||
        prev_settings.wifi.ssid != new_settings.wifi.ssid ||
        prev_settings.wifi.passwd != new_settings.wifi.passwd;

    JsonDocument response;

    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }

    response["cmd"] = "save_config";
    response["saved_keys"] = saved_keys;
    response["reboot_needed"] = reboot_needed;
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

void cmdSyncNtp(JsonDocument& request, Stream& output) {
    if (!wifiManager.isConnected()) {
        sendError("sync_ntp", 204, "WiFi not connected", request["id"], output);
        ESP_LOGW(TAG, "Sync_ntp failed: WiFi not connected");
        return;
    }

    if (!rtc.isReady()) {
        sendError("sync_ntp", 201, "RTC not initialized", request["id"], output);
        ESP_LOGW(TAG, "Sync_ntp failed: RTC not initialized");
        return;
    }

    uint32_t duration_ms = 0;
    const SyncSettings &sync = settings.getSync();
    const char *ntp1 = (sync.ntp1.length() > 0) ? sync.ntp1.c_str() : DEFAULT_SYNC_NTP1;
    const char *ntp2 = (sync.ntp2.length() > 0) ? sync.ntp2.c_str() : DEFAULT_SYNC_NTP2;
    const char *ntp3 = (sync.ntp3.length() > 0) ? sync.ntp3.c_str() : DEFAULT_SYNC_NTP3;
    bool ok = syncRtcUtcFromNtpPrecise(
        rtc,
        wifiManager,
        ntp1,
        ntp2,
        ntp3,
        SNTP_SYNC_TIMEOUT_MS,
        SNTP_EDGE_TIMEOUT_MS,
        SNTP_EDGE_WINDOW_US,
        &duration_ms
    );

    if (!ok) {
        sendError("sync_ntp", 203, "NTP sync failed", request["id"], output);
        ESP_LOGW(TAG, "Sync_ntp failed: unknown error during sync");
        return;
    }

    JsonDocument response;

    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }

    response["cmd"] = "sync_ntp";
    response["status"] = "ok";
    response["rtc_time"] = rtc.unixTime();
    response["ntp_server"] = ntp1;
    response["sync_duration_ms"] = duration_ms;

    sendResponse(response, output, true);

    ESP_LOGI(TAG, "Sync_ntp command processed: duration=%lu ms",
             (unsigned long)duration_ms);
}

void cmdWifi(JsonDocument& request, Stream& output) {
    if (request["enable"].isNull()) {
        sendError("wifi", 102, "Missing 'enable' field", request["id"], output);
        ESP_LOGW(TAG, "WiFi command failed: missing enable");
        return;
    }

    if (!request["enable"].is<bool>()) {
        sendError("wifi", 103, "Invalid 'enable' field type", request["id"], output);
        ESP_LOGW(TAG, "WiFi command failed: invalid enable type");
        return;
    }

    bool enable = request["enable"].as<bool>();
    if (!enable) {
        bool stopped = wifiManager.end();
        if (!stopped) {
            sendError("wifi", 206, "WiFi stop timeout", request["id"], output);
            ESP_LOGW(TAG, "WiFi stop timeout");
            return;
        }

        JsonDocument response;
        if (!request["id"].isNull()) {
            response["id"] = request["id"];
        }

        response["cmd"] = "wifi";
        response["state"] = "disabled";
        response["status"] = "ok";

        sendResponse(response, output, true);
        ESP_LOGI(TAG, "WiFi disabled via command");
        return;
    }

    String ssid;
    String passwd;
    bool has_ssid = false;
    if (!request["ssid"].isNull()) {
        ssid = request["ssid"].as<String>();
        has_ssid = ssid.length() > 0;
        if (!request["passwd"].isNull()) {
            passwd = request["passwd"].as<String>();
        }
    }

    if (!has_ssid) {
        const WifiSettings &wifi = settings.getWifi();
        ssid = wifi.ssid;
        passwd = wifi.passwd;
        has_ssid = ssid.length() > 0;
    }

    if (!has_ssid) {
        sendError("wifi", 102, "Missing 'ssid' field", request["id"], output);
        ESP_LOGW(TAG, "WiFi command failed: missing ssid");
        return;
    }

    wifiManager.begin();
    if (!wifiManager.connect(ssid, passwd)) {
        sendError("wifi", 205, "WiFi start failed", request["id"], output);
        ESP_LOGW(TAG, "WiFi command failed: connect start failed");
        return;
    }

    JsonDocument response;
    if (!request["id"].isNull()) {
        response["id"] = request["id"];
    }

    response["cmd"] = "wifi";
    response["state"] = "enabled";
    response["status"] = "ok";

    sendResponse(response, output, true);
    ESP_LOGI(TAG, "WiFi enabled via command: ssid=%s", ssid.c_str());
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
