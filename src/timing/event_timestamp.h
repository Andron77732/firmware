#pragma once
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

/**
 * @brief Структура данных временного штампа события
 */
struct EventTimestampData {
    bool success;              // Успешна ли обработка
    int64_t esp_timestamp_us;  // Исходный ESP timestamp
    int64_t utc_timestamp_us;  // UTC timestamp (если успешно)
    
    // Локальное время (разобранное)
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
    
    // Форматированная строка времени
    String local_time_str;     // "HH:MM:SS,mmm"
};

/**
 * @brief Обработка временного штампа события
 * 
 * Конвертирует временной штамп ESP (микросекунды) в UTC и локальное время,
 * возвращает структурированные данные.
 * 
 * @param esp_timestamp_us Временной штамп ESP в микросекундах
 * @param module_type Тип модуля (START или FINISH) - используется только для логирования
 * @return EventTimestampData Структура с данными события (success=false если источник времени не готов)
 */
EventTimestampData event_timestamp_process(int64_t esp_timestamp_us, ModuleType module_type);

/**
 * @brief Отправка данных события в BLE (заглушка)
 * 
 * В текущей реализации только логирует событие, не выполняет реальную отправку.
 * 
 * @param data Данные временного штампа
 */
void event_timestamp_send_ble(const EventTimestampData& data);

