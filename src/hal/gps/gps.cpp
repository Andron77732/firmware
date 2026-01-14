#include "gps.h"
#include "config.h"
#include "esp_log.h"
#include <esp_timer.h>

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
    updateState_();
}

void GPS::update() {
    while (gpsSerial.available()) {
        char c = gpsSerial.read();

        // Мягкий таймстампинг: фиксируем старт предложения по '$'
        if (c == '$') {
            _in_sentence = true;
            _current_sentence_start_us = esp_timer_get_time();
        }
        
        // Собираем строку для verbose лога
        if (c == '\n' || c == '\r') {
            if (verboseIdx > 0) {
                verboseLine[verboseIdx] = '\0';
                ESP_LOGV(TAG_NMEA, "%s", verboseLine);
                verboseIdx = 0;
            }

            if (_in_sentence) {
                _last_sentence_start_us = _current_sentence_start_us;
                _current_sentence_start_us = 0;
                _in_sentence = false;
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

    updateState_();
}

bool GPS::lastSentenceStartUs(int64_t &ts_us) const {
    if (_last_sentence_start_us == 0) return false;
    ts_us = _last_sentence_start_us;
    return true;
}

GPSState GPS::getState() const {
    if (!_initialized) return GPSState::OFF;
    return _nmea.isValid() ? GPSState::ACTIVE : GPSState::SEARCHING;
}

void GPS::setStateCallback(void (*callback)(GPSState state)) {
    _stateCallback = callback;
    _lastState = getState();
    notifyStateChanged_();
}

void GPS::notifyStateChanged_() {
    if (_stateCallback) {
        _stateCallback(getState());
    }
}

void GPS::updateState_() {
    GPSState state = getState();
    if (state == _lastState) return;
    _lastState = state;
    notifyStateChanged_();
}
