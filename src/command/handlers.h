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

#endif // COMMAND_HANDLERS_H
