#ifndef SETTINGS_H
#define SETTINGS_H

#include <Preferences.h>
#include <Arduino.h>

// ============================================================================
// Значения по умолчанию
// ============================================================================

#define DEFAULT_DEVICE_NAME "ENTime"
#define DEFAULT_DEVICE_NUMBER 1
#define DEFAULT_DEVICE_TYPE 1
#define DEFAULT_DEVICE_TIMEZONE 3

#define DEFAULT_SYNC_AUTO true
#define DEFAULT_SYNC_SOURCE 0

#define DEFAULT_WIFI_ACTIVE false
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWD ""

#define DEFAULT_CALIBRATION_RTC_OFFSET_PPM 0.0f

// ============================================================================
// Структуры данных
// ============================================================================

/**
 * @brief Настройки устройства
 */
struct DeviceSettings {
  String name = DEFAULT_DEVICE_NAME;    // 1-16 символов, ASCII
  uint8_t number = DEFAULT_DEVICE_NUMBER;    // 1-255
  uint8_t type = DEFAULT_DEVICE_TYPE;        // 1 = стартовый, 2 = финишный
  int8_t timezone = DEFAULT_DEVICE_TIMEZONE; // -12 до 12
};

/**
 * @brief Настройки синхронизации времени
 */
struct SyncSettings {
  bool auto_sync = DEFAULT_SYNC_AUTO;       // автоматическая синхронизация
  uint8_t source = DEFAULT_SYNC_SOURCE;     // 0 = авто, 1 = GPS, 2 = RTC
};

/**
 * @brief Настройки WiFi
 */
struct WifiSettings {
  bool active = DEFAULT_WIFI_ACTIVE;        // включен ли WiFi
  String ssid = DEFAULT_WIFI_SSID;     // до 32 символов
  String passwd = DEFAULT_WIFI_PASSWD; // до 64 символов
};

/**
 * @brief Настройки калибровки
 */
struct CalibrationSettings {
  float rtc_offset_ppm =
      DEFAULT_CALIBRATION_RTC_OFFSET_PPM; // -100.0 до 100.0 ppm
};

/**
 * @brief Полная структура настроек
 */
struct Settings {
  DeviceSettings device;
  SyncSettings sync;
  WifiSettings wifi;
  CalibrationSettings calibration;
};

/**
 * @brief Менеджер настроек для работы с NVS
 *
 * Хранит настройки в ESP32 NVS через Preferences API.
 * Поддерживает загрузку, сохранение, валидацию и обновление только изменившихся настроек.
 */
class SettingsManager {
public:
  /**
   * @brief Инициализация менеджера и загрузка настроек из NVS
   * @return true если успешно, false при ошибке
   */
  bool begin();

  /**
   * @brief Загрузить все настройки из NVS
   * @return true если успешно, false при ошибке
   */
  bool load();

  /**
   * @brief Сохранить все настройки в NVS
   * @return true если успешно, false при ошибке
   */
  bool save();

  /**
   * @brief Сброс всех настроек к значениям по умолчанию
   * @return true если успешно, false при ошибке
   */
  bool factoryReset();

  // Геттеры
  const DeviceSettings &getDevice() const { return settings_.device; }
  const SyncSettings &getSync() const { return settings_.sync; }
  const WifiSettings &getWifi() const { return settings_.wifi; }
  const CalibrationSettings &getCalibration() const {
    return settings_.calibration;
  }
  const Settings &getAll() const { return settings_; }

  // Сеттеры с валидацией
  /**
   * @brief Установить настройки устройства с валидацией
   * @param device Настройки устройства
   * @return true если валидация прошла и настройки установлены, false при
   * ошибке
   */
  bool setDevice(const DeviceSettings &device);

  /**
   * @brief Установить настройки синхронизации с валидацией
   * @param sync Настройки синхронизации
   * @return true если валидация прошла и настройки установлены, false при
   * ошибке
   */
  bool setSync(const SyncSettings &sync);

  /**
   * @brief Установить настройки WiFi с валидацией
   * @param wifi Настройки WiFi
   * @return true если валидация прошла и настройки установлены, false при
   * ошибке
   */
  bool setWifi(const WifiSettings &wifi);

  /**
   * @brief Установить настройки калибровки с валидацией
   * @param calibration Настройки калибровки
   * @return true если валидация прошла и настройки установлены, false при
   * ошибке
   */
  bool setCalibration(const CalibrationSettings &calibration);

  /**
   * @brief Получить статистику использования хранилища
   * @param used_bytes Количество использованных байт
   * @param total_bytes Общий размер раздела
   * @return true если успешно
   */
  bool getStorageStats(size_t &used_bytes, size_t &total_bytes) const;

private:
  Preferences prefs_;
  Settings settings_;
  bool initialized_ = false;

  // Хелперы для записи только при изменении (уменьшение износа NVS + единый формат логирования)
  bool putStringIfChanged_(const char *key, const String &v,
                           const char *def, bool secret = false);
  bool putBoolIfChanged_(const char *key, bool v, bool def);
  bool putUCharIfChanged_(const char *key, uint8_t v, uint8_t def);
  bool putCharIfChanged_(const char *key, int8_t v, int8_t def);
  bool putFloatIfChanged_(const char *key, float v, float def,
                          float eps = 0.0001f);

  // Валидация
  bool validateDevice(const DeviceSettings &device) const;
  bool validateSync(const SyncSettings &sync) const;
  bool validateWifi(const WifiSettings &wifi) const;
  bool validateCalibration(const CalibrationSettings &calibration) const;

  // Вспомогательные функции валидации
  bool isValidAsciiName(const String &name) const;
  bool isValidDeviceType(uint8_t type) const;
  bool isValidSyncSource(uint8_t source) const;

  // Сохранение/загрузка отдельных групп
  void saveDevice();
  void saveSync();
  void saveWifi();
  void saveCalibration();

  void loadDevice();
  void loadSync();
  void loadWifi();
  void loadCalibration();

  // Установка значений по умолчанию
  void setDefaults();
};

// Глобальный экземпляр менеджера настроек
extern SettingsManager settings;

#endif // SETTINGS_H
