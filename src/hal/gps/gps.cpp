#include "gps.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = "GPS";
static const char* TAG_NMEA = "GPS][NMEA";  // Хак для вывода [GPS][NMEA]

// Глобальный объект GPS
GPS gps;

// GPS Serial (UART2)
static HardwareSerial gpsSerial(2);

// Буфер для verbose вывода NMEA строки
static char verboseLine[128];
static uint8_t verboseIdx = 0;

void GPS::begin() {
    if (_initialized) return;
    
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    ESP_LOGI(TAG, "Initialized (RX:%d, TX:%d, PPS:%d)", GPS_RX_PIN, GPS_TX_PIN, GPS_PPS_PIN);
    
    _initialized = true;
}

void GPS::update() {
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        
        // Собираем строку для verbose лога
        if (c == '\n' || c == '\r') {
            if (verboseIdx > 0) {
                verboseLine[verboseIdx] = '\0';
                ESP_LOGV(TAG_NMEA, "%s", verboseLine);
                verboseIdx = 0;
            }
        } else if (verboseIdx < sizeof(verboseLine) - 1) {
            verboseLine[verboseIdx++] = c;
        } else {
            // Переполнение — сбрасываем
            verboseIdx = 0;
        }
        
        // Парсинг NMEA
        _nmea.process(c);
    }
}
