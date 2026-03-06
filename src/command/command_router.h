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
     * @param output Поток для отправки ответа (Serial, NUS и т.д.)
     */
    static void route(JsonDocument& doc, Stream& output);
};

#endif // COMMAND_ROUTER_H
