#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Парсер JSON команд из Stream
 * 
 * Класс для парсинга JSON команд из любого Stream (Serial, BLE и т.д.)
 * с поддержкой буферизации для обработки фрагментированных и многострочных JSON.
 */
class CommandParser {
public:
    /**
     * @brief Конструктор парсера
     * @param stream Поток для чтения данных (Serial, BLESerial и т.д.)
     * @param sourceName Имя источника для логирования (например, "Serial" или "BLE")
     * @param bufferSize Размер буфера для накопления данных (по умолчанию 1024 байт)
     */
    CommandParser(Stream& stream, const char* sourceName = "Unknown", size_t bufferSize = 1024);
    
    /**
     * @brief Деструктор - освобождает буфер
     */
    ~CommandParser();
    
    /**
     * @brief Обновление парсера - вызывать в loop()
     * 
     * Читает доступные данные из Stream, накапливает в буфере,
     * определяет полный JSON и парсит его.
     */
    void update();

private:
    /**
     * @brief Обработка JSON команды из буфера (вызывается при обнаружении \n)
     */
    void processJson();
    
    /**
     * @brief Очистка буфера
     */
    void clearBuffer();

private:
    Stream* _stream;              // Поток для чтения данных
    const char* _sourceName;       // Имя источника для логирования
    char* _buffer;                 // Буфер для накопления данных
    size_t _bufferSize;            // Размер буфера
    size_t _bufferPos;             // Текущая позиция в буфере

    uint32_t _frameStartMs = 0;     // Время начала приёма кадра
    static constexpr uint32_t FRAME_TIMEOUT_MS = 1000; // Таймаут
};

#endif // COMMAND_PARSER_H

