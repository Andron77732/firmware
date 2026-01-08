#ifndef COMMAND_ROUTER_H
#define COMMAND_ROUTER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Тип функции-обработчика команды
typedef void (*CommandHandler)(JsonDocument& request, Stream& output);

// Структура записи в таблице маршрутизации
struct CommandRoute {
    const char* name;        // Имя команды (строка из JSON)
    CommandHandler handler;   // Функция-обработчик
};

/**
 * @brief Роутер для маршрутизации JSON команд к соответствующим обработчикам
 * 
 * Класс для маршрутизации распарсенных JSON команд к обработчикам
 * и отправки ответов обратно через Stream.
 */
class CommandRouter {
public:
    /**
     * @brief Маршрутизация команды к соответствующему обработчику
     * @param doc Распарсенный JSON документ с командой
     * @param output Поток для отправки ответа (Serial, BLESerial и т.д.)
     */
    static void route(JsonDocument& doc, Stream& output);

private:
    /**
     * @brief Отправка ответа об ошибке
     * @param cmd Имя команды
     * @param errorCode Код ошибки
     * @param errorMessage Сообщение об ошибке
     * @param requestId ID запроса (если есть)
     * @param output Поток для отправки
     */
    static void sendError(const char* cmd, int errorCode, const char* errorMessage, 
                         const JsonVariant& requestId, Stream& output);
};

#endif // COMMAND_ROUTER_H
