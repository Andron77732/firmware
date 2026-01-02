#include "gps.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = "GPS";
static const char* TAG_NMEA = "GPS][NMEA";  // Хак для вывода [GPS][NMEA]

// Глобальный объект GPS
GPS gps;

// GPS Serial (UART2)
static HardwareSerial gpsSerial(2);

// Буфер для debug вывода NMEA строки
static char debugLine[128];
static uint8_t debugIdx = 0;

void GPS::begin() {
    if (_initialized) return;
    
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    ESP_LOGI(TAG, "Initialized (RX:%d, TX:%d, PPS:%d)", GPS_RX_PIN, GPS_TX_PIN, GPS_PPS_PIN);
    
    _initialized = true;
}

void GPS::update() {
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        
        // Собираем строку для debug лога
        if (c == '\n' || c == '\r') {
            if (debugIdx > 0) {
                debugLine[debugIdx] = '\0';
                ESP_LOGD(TAG_NMEA, "%s", debugLine);
                debugIdx = 0;
            }
        } else if (debugIdx < sizeof(debugLine) - 1) {
            debugLine[debugIdx++] = c;
        } else {
            // Переполнение — сбрасываем
            debugIdx = 0;
        }
        
        // Парсинг NMEA
        _nmea.process(c);
    }
}
