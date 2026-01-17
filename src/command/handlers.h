#ifndef COMMAND_HANDLERS_H
#define COMMAND_HANDLERS_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Обработчик команды ping
 * 
 * Простая команда для проверки связи. Всегда возвращает успешный ответ pong.
 * 
 * @param request JSON запрос с полем "cmd": "ping"
 * @param output Поток для отправки ответа
 */
void cmdPing(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды time
 * 
 * Возвращает текущее время в микросекундах с момента UNIX epoch,
 * источник времени (GPS или RTC) и точность.
 * 
 * @param request JSON запрос с полем "cmd": "time"
 * @param output Поток для отправки ответа
 */
void cmdTime(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды status
 *
 * Возвращает базовый статус устройства (включая версию и дату сборки).
 *
 * @param request JSON запрос с полем "cmd": "status"
 * @param output Поток для отправки ответа
 */
void cmdStatus(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды load_config
 *
 * Возвращает текущие настройки устройства в JSON формате.
 *
 * @param request JSON запрос с полем "cmd": "load_config"
 * @param output Поток для отправки ответа
 */
void cmdLoadConfig(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды save_config
 *
 * Сохраняет настройки устройства из поля data.
 *
 * @param request JSON запрос с полем "cmd": "save_config"
 * @param output Поток для отправки ответа
 */
void cmdSaveConfig(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды sync_ntp
 *
 * Выполняет синхронизацию RTC по NTP и возвращает параметры результата.
 *
 * @param request JSON запрос с полем "cmd": "sync_ntp"
 * @param output Поток для отправки ответа
 */
void cmdSyncNtp(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды wifi
 *
 * Включает или выключает WiFi, опционально задает SSID и пароль.
 *
 * @param request JSON запрос с полем "cmd": "wifi"
 * @param output Поток для отправки ответа
 */
void cmdWifi(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды sync_source
 *
 * Переключает источник синхронизации (gps/rtc) и возвращает активный источник.
 *
 * @param request JSON запрос с полем "cmd": "sync_source"
 * @param output Поток для отправки ответа
 */
void cmdSyncSource(JsonDocument& request, Stream& output);

/**
 * @brief Обработчик команды factory_reset
 *
 * Сбрасывает все настройки к значениям по умолчанию и планирует перезагрузку.
 *
 * @param request JSON запрос с полем "cmd": "factory_reset"
 * @param output Поток для отправки ответа
 */
void cmdFactoryReset(JsonDocument& request, Stream& output);

/**
 * @brief Утилита для отправки JSON-ответа в поток
 * 
 * Сериализует переданный JSON-документ (response) в строку, 
 * добавляет перевод строки, и записывает результат в указанный поток output.
 * Используется для отправки стандартных ответов клиенту.
 * 
 * @param response JSON-документ с ответом
 * @param output Поток, в который отправляется ответ
 * @param addNewline Добавить перевод строки в конец ответа (по умолчанию true)
 */
void sendResponse(JsonDocument& response, Stream& output, bool addNewline = true);

/**
 * @brief Утилита для отправки ответа об ошибке
 * 
 * Формирует стандартный JSON-ответ об ошибке и отправляет его в поток.
 * Используется обработчиками команд и роутером для единообразной обработки ошибок.
 * 
 * @param cmd Имя команды
 * @param errorCode Код ошибки
 * @param errorMessage Сообщение об ошибке
 * @param requestId ID запроса (если есть)
 * @param output Поток для отправки
 */
void sendError(const char* cmd, int errorCode, const char* errorMessage, 
               const JsonVariant& requestId, Stream& output);

#endif // COMMAND_HANDLERS_H
