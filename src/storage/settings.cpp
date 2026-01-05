#include "settings.h"
#include "esp_log.h"
#include "nvs.h"
#include <cctype>
#include <cmath>

static const char *TAG = "Settings";

// ============================================================================
// NVS change logging (единый формат)
// ============================================================================

#define SETTINGS_LOG_NVS_FMT(key, fmt, ...)                                    \
  ESP_LOGD(TAG, "NVS: %-28s = " fmt, key, ##__VA_ARGS__)

#define SETTINGS_LOG_NVS_SECRET(key) ESP_LOGD(TAG, "NVS: %-28s = <secret>", key)

#define SETTINGS_LOG_NVS_SKIP(key) ESP_LOGV(TAG, "NVS: %-28s (unchanged)", key)

// Глобальный экземпляр
SettingsManager settings;

// ============================================================================
// Валидация
// ============================================================================

bool SettingsManager::isValidAsciiName(const String &name) const {
  if (name.length() == 0 || name.length() > 16) {
    return false;
  }

  for (uint16_t i = 0; i < name.length(); i++) {
    char c = name.charAt(i);
    // важно: cast в unsigned char, иначе возможен UB на signed char
    if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') {
      return false;
    }
  }
  return true;
}

bool SettingsManager::isValidDeviceType(uint8_t type) const {
  return type == 1 || type == 2;
}

bool SettingsManager::isValidSyncSource(uint8_t source) const {
  return source == 0 || source == 1 || source == 2;
}

bool SettingsManager::validateDevice(const DeviceSettings &device) const {
  if (!isValidAsciiName(device.name)) {
    ESP_LOGE(TAG, "Invalid device.name: must be 1-16 ASCII characters (a-z, "
                  "A-Z, 0-9, -, _)");
    return false;
  }

  if (device.number < 1 || device.number > 255) {
    ESP_LOGE(TAG, "Invalid device.number: must be 1-255, got %u",
             device.number);
    return false;
  }

  if (!isValidDeviceType(device.type)) {
    ESP_LOGE(TAG,
             "Invalid device.type: must be 1 (start) or 2 (finish), got %u",
             device.type);
    return false;
  }

  if (device.timezone < -12 || device.timezone > 12) {
    ESP_LOGE(TAG, "Invalid device.timezone: must be -12 to 12, got %d",
             device.timezone);
    return false;
  }

  return true;
}

bool SettingsManager::validateSync(const SyncSettings &sync) const {
  if (!isValidSyncSource(sync.source)) {
    ESP_LOGE(
        TAG,
        "Invalid sync.source: must be 0 (auto), 1 (gps), or 2 (rtc), got %u",
        sync.source);
    return false;
  }

  return true;
}

bool SettingsManager::validateWifi(const WifiSettings &wifi) const {
  if (wifi.ssid.length() > 32) {
    ESP_LOGE(TAG, "Invalid wifi.ssid: must be <= 32 characters, got %u",
             wifi.ssid.length());
    return false;
  }

  if (wifi.passwd.length() > 64) {
    ESP_LOGE(TAG, "Invalid wifi.passwd: must be <= 64 characters, got %u",
             wifi.passwd.length());
    return false;
  }

  return true;
}

bool SettingsManager::validateCalibration(
    const CalibrationSettings &calibration) const {
  if (calibration.rtc_offset_ppm < -100.0f ||
      calibration.rtc_offset_ppm > 100.0f) {
    ESP_LOGE(
        TAG,
        "Invalid calibration.rtc_offset_ppm: must be -100.0 to 100.0, got %.1f",
        calibration.rtc_offset_ppm);
    return false;
  }

  return true;
}

// ============================================================================
// Defaults
// ============================================================================

void SettingsManager::setDefaults() {
  settings_.device.name = DEFAULT_DEVICE_NAME;
  settings_.device.number = DEFAULT_DEVICE_NUMBER;
  settings_.device.type = DEFAULT_DEVICE_TYPE;
  settings_.device.timezone = DEFAULT_DEVICE_TIMEZONE;

  settings_.sync.auto_sync = DEFAULT_SYNC_AUTO;
  settings_.sync.source = DEFAULT_SYNC_SOURCE;

  settings_.wifi.active = DEFAULT_WIFI_ACTIVE;
  settings_.wifi.ssid = DEFAULT_WIFI_SSID;
  settings_.wifi.passwd = DEFAULT_WIFI_PASSWD;

  settings_.calibration.rtc_offset_ppm = DEFAULT_CALIBRATION_RTC_OFFSET_PPM;
}

// ============================================================================
// Write-if-changed helpers (уменьшают износ NVS + debug лог)
// ============================================================================

bool SettingsManager::putStringIfChanged_(const char *key, const String &v,
                                          const char *def, bool secret) {
  String cur = prefs_.getString(key, def);
  if (cur == v) {
    SETTINGS_LOG_NVS_SKIP(key);
    return false;
  }

  prefs_.putString(key, v);

  if (secret) {
    SETTINGS_LOG_NVS_SECRET(key);
  } else {
    SETTINGS_LOG_NVS_FMT(key, "\"%s\"", v.c_str());
  }
  return true;
}

bool SettingsManager::putBoolIfChanged_(const char *key, bool v, bool def) {
  bool cur = prefs_.getBool(key, def);
  if (cur == v) {
    SETTINGS_LOG_NVS_SKIP(key);
    return false;
  }

  prefs_.putBool(key, v);
  SETTINGS_LOG_NVS_FMT(key, "%s", v ? "true" : "false");
  return true;
}

bool SettingsManager::putUCharIfChanged_(const char *key, uint8_t v,
                                         uint8_t def) {
  uint8_t cur = prefs_.getUChar(key, def);
  if (cur == v) {
    SETTINGS_LOG_NVS_SKIP(key);
    return false;
  }

  prefs_.putUChar(key, v);
  SETTINGS_LOG_NVS_FMT(key, "%u", v);
  return true;
}

bool SettingsManager::putCharIfChanged_(const char *key, int8_t v, int8_t def) {
  int8_t cur = prefs_.getChar(key, def);
  if (cur == v) {
    SETTINGS_LOG_NVS_SKIP(key);
    return false;
  }

  prefs_.putChar(key, v);
  SETTINGS_LOG_NVS_FMT(key, "%d", v);
  return true;
}

bool SettingsManager::putFloatIfChanged_(const char *key, float v, float def,
                                         float eps) {
  float cur = prefs_.getFloat(key, def);
  if (std::fabs(cur - v) <= eps) {
    SETTINGS_LOG_NVS_SKIP(key);
    return false;
  }

  prefs_.putFloat(key, v);
  SETTINGS_LOG_NVS_FMT(key, "%.4f", v);
  return true;
}

// ============================================================================
// Сохранение/загрузка отдельных групп
// ============================================================================

void SettingsManager::saveDevice() {
  putStringIfChanged_("device.name", settings_.device.name,
                      DEFAULT_DEVICE_NAME);
  putUCharIfChanged_("device.number", settings_.device.number,
                     DEFAULT_DEVICE_NUMBER);
  putUCharIfChanged_("device.type", settings_.device.type, DEFAULT_DEVICE_TYPE);
  putCharIfChanged_("device.timezone", settings_.device.timezone,
                    DEFAULT_DEVICE_TIMEZONE);
}

void SettingsManager::saveSync() {
  putBoolIfChanged_("sync.auto", settings_.sync.auto_sync, DEFAULT_SYNC_AUTO);
  putUCharIfChanged_("sync.source", settings_.sync.source, DEFAULT_SYNC_SOURCE);
}

void SettingsManager::saveWifi() {
  putBoolIfChanged_("wifi.active", settings_.wifi.active, DEFAULT_WIFI_ACTIVE);
  putStringIfChanged_("wifi.ssid", settings_.wifi.ssid, DEFAULT_WIFI_SSID);
  // пароль не логируем значением
  putStringIfChanged_("wifi.passwd", settings_.wifi.passwd, DEFAULT_WIFI_PASSWD,
                      true);
}

void SettingsManager::saveCalibration() {
  putFloatIfChanged_("calibration.rtc_offset_ppm",
                     settings_.calibration.rtc_offset_ppm,
                     DEFAULT_CALIBRATION_RTC_OFFSET_PPM, 0.0001f);
}

void SettingsManager::loadDevice() {
  settings_.device.name = prefs_.getString("device.name", DEFAULT_DEVICE_NAME);

  settings_.device.number =
      prefs_.getUChar("device.number", DEFAULT_DEVICE_NUMBER);

  settings_.device.type = prefs_.getUChar("device.type", DEFAULT_DEVICE_TYPE);

  settings_.device.timezone =
      prefs_.getChar("device.timezone", DEFAULT_DEVICE_TIMEZONE);
}

void SettingsManager::loadSync() {
  settings_.sync.auto_sync = prefs_.getBool("sync.auto", DEFAULT_SYNC_AUTO);

  settings_.sync.source = prefs_.getUChar("sync.source", DEFAULT_SYNC_SOURCE);
}

void SettingsManager::loadWifi() {
  settings_.wifi.active = prefs_.getBool("wifi.active", DEFAULT_WIFI_ACTIVE);

  settings_.wifi.ssid = prefs_.getString("wifi.ssid", DEFAULT_WIFI_SSID);

  settings_.wifi.passwd = prefs_.getString("wifi.passwd", DEFAULT_WIFI_PASSWD);
}

void SettingsManager::loadCalibration() {
  settings_.calibration.rtc_offset_ppm = prefs_.getFloat(
      "calibration.rtc_offset_ppm", DEFAULT_CALIBRATION_RTC_OFFSET_PPM);
}

// ============================================================================
// Публичные методы
// ============================================================================

bool SettingsManager::begin() {
  if (initialized_) {
    ESP_LOGW(TAG, "SettingsManager already initialized");
    return true;
  }

  if (!prefs_.begin("entime", false)) {
    ESP_LOGE(TAG, "Failed to open NVS namespace 'entime'");
    return false;
  }

  initialized_ = true;
  ESP_LOGI(TAG, "SettingsManager initialized");

  // Загружаем настройки (или используем значения по умолчанию)
  if (!load()) {
    ESP_LOGW(TAG, "Failed to load settings, using defaults");
    setDefaults();
  }

  return true;
}

bool SettingsManager::load() {
  if (!initialized_) {
    ESP_LOGE(TAG, "SettingsManager not initialized, call begin() first");
    return false;
  }

  // Если ключ "device.name" не существует — настроек нет
  if (!prefs_.isKey("device.name")) {
    ESP_LOGI(TAG, "No saved settings found, using defaults");
    setDefaults();
    return true;
  }

  loadDevice();
  loadSync();
  loadWifi();
  loadCalibration();

  // Валидация загруженных настроек
  if (!validateDevice(settings_.device) || !validateSync(settings_.sync) ||
      !validateWifi(settings_.wifi) ||
      !validateCalibration(settings_.calibration)) {
    ESP_LOGW(TAG, "Loaded settings failed validation, using defaults");
    setDefaults();
    return false;
  }

  ESP_LOGI(TAG, "Settings loaded successfully");
  return true;
}

bool SettingsManager::save() {
  if (!initialized_) {
    ESP_LOGE(TAG, "SettingsManager not initialized, call begin() first");
    return false;
  }

  // Валидация перед сохранением
  if (!validateDevice(settings_.device) || !validateSync(settings_.sync) ||
      !validateWifi(settings_.wifi) ||
      !validateCalibration(settings_.calibration)) {
    ESP_LOGE(TAG, "Settings validation failed, cannot save");
    return false;
  }

  saveDevice();
  saveSync();
  saveWifi();
  saveCalibration();

  ESP_LOGI(TAG, "Settings saved successfully");
  return true;
}

bool SettingsManager::factoryReset() {
  if (!initialized_) {
    ESP_LOGE(TAG, "SettingsManager not initialized, call begin() first");
    return false;
  }

  prefs_.clear();
  setDefaults();

  if (!save()) {
    ESP_LOGE(TAG, "Failed to save default settings after factory reset");
    return false;
  }

  ESP_LOGI(TAG, "Factory reset completed");
  return true;
}

bool SettingsManager::setDevice(const DeviceSettings &device) {
  if (!validateDevice(device)) {
    return false;
  }
  settings_.device = device;
  return true;
}

bool SettingsManager::setSync(const SyncSettings &sync) {
  if (!validateSync(sync)) {
    return false;
  }
  settings_.sync = sync;
  return true;
}

bool SettingsManager::setWifi(const WifiSettings &wifi) {
  if (!validateWifi(wifi)) {
    return false;
  }
  settings_.wifi = wifi;
  return true;
}

bool SettingsManager::setCalibration(const CalibrationSettings &calibration) {
  if (!validateCalibration(calibration)) {
    return false;
  }
  settings_.calibration = calibration;
  return true;
}

bool SettingsManager::getStorageStats(size_t &used_bytes,
                                      size_t &total_bytes) const {
  if (!initialized_) {
    return false;
  }

  // Получаем статистику NVS раздела
  nvs_stats_t nvs_stats;
  esp_err_t err = nvs_get_stats("nvs", &nvs_stats);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(err));
    return false;
  }

  // nvs_stats содержит статистику по записям (entries), а не по байтам
  // Каждая запись в NVS имеет фиксированный размер: 32 байта (ключ + значение +
  // метаданные) Но размер может варьироваться в зависимости от типа данных
  const size_t nvs_entry_size =
      32; // стандартный размер одной записи NVS в байтах

  // Общий размер раздела = общее количество записей * размер записи
  total_bytes = nvs_stats.total_entries * nvs_entry_size;

  // Использованное пространство = использованные записи * размер записи
  used_bytes = nvs_stats.used_entries * nvs_entry_size;

  ESP_LOGD(TAG,
           "NVS stats: used_entries=%lu, free_entries=%lu, "
           "total_entries=%lu, namespace_count=%lu, "
           "used_bytes=%zu, total_bytes=%zu",
           (unsigned long)nvs_stats.used_entries,
           (unsigned long)nvs_stats.free_entries,
           (unsigned long)nvs_stats.total_entries,
           (unsigned long)nvs_stats.namespace_count, used_bytes, total_bytes);

  return true;
}
