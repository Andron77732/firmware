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
    updateSats_();
}

void GPS::update() {
    while (gpsSerial.available()) {
        const int64_t now_us = esp_timer_get_time();
        char c = gpsSerial.read();

        // Мягкий таймстампинг: фиксируем старт предложения по '$'
        if (c == '$') {
            _in_sentence = true;
            _current_sentence_start_us = now_us;
        }

        const int64_t sentence_start_us = _in_sentence ? _current_sentence_start_us : 0;
        uint64_t utc_before = 0;
        const bool had_utc_before = readUtcSignature_(utc_before);
        
        // Собираем строку для verbose лога
        if (c == '\n' || c == '\r') {
            if (verboseIdx > 0) {
                verboseLine[verboseIdx] = '\0';
                ESP_LOGV(TAG_NMEA, "%s", verboseLine);
                verboseIdx = 0;
            }

            if (_in_sentence) {
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
        const bool processed_sentence = _nmea.process(c);
        if (processed_sentence) {
            _last_sentence_us = now_us;
            if (_nmea.isValid()) {
                _last_fix_us = now_us;
            }
        }

        uint64_t utc_after = 0;
        if (processed_sentence &&
            sentence_start_us != 0 &&
            readUtcSignature_(utc_after) &&
            (!had_utc_before || utc_after != utc_before) &&
            utc_after != _last_utc_signature) {
            _last_utc_signature = utc_after;
            _last_utc_update_sentence_start_us = sentence_start_us;
            _last_utc_update_us = now_us;
        }
    }

    updateState_();
    updateSats_();
}

bool GPS::lastUtcUpdateSentenceStartUs(int64_t &ts_us) const {
    if (_last_utc_update_sentence_start_us == 0) return false;
    ts_us = _last_utc_update_sentence_start_us;
    return true;
}

bool GPS::lastSentenceUs(int64_t &ts_us) const {
    if (_last_sentence_us == 0) return false;
    ts_us = _last_sentence_us;
    return true;
}

bool GPS::lastUtcUpdateUs(int64_t &ts_us) const {
    if (_last_utc_update_us == 0) return false;
    ts_us = _last_utc_update_us;
    return true;
}

bool GPS::nmeaFresh(int64_t max_age_us) const {
    if (!_initialized || _last_sentence_us == 0) return false;
    int64_t age_us = esp_timer_get_time() - _last_sentence_us;
    return age_us >= 0 && age_us <= max_age_us;
}

bool GPS::utcFresh(int64_t max_age_us) const {
    if (!_initialized || _last_utc_update_us == 0) return false;
    int64_t age_us = esp_timer_get_time() - _last_utc_update_us;
    return age_us >= 0 && age_us <= max_age_us;
}

GPSState GPS::getState() const {
    if (!_initialized) return GPSState::OFF;
    return (_nmea.isValid() && nmeaFresh()) ? GPSState::ACTIVE : GPSState::SEARCHING;
}

void GPS::setStateCallback(void (*callback)(GPSState state)) {
    _stateCallback = callback;
    _lastState = getState();
    notifyStateChanged_();
}

void GPS::setSatsCallback(void (*callback)(int8_t sats)) {
    _satsCallback = callback;
    _lastSats = currentSats_();
    notifySatsChanged_();
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

void GPS::notifySatsChanged_() {
    if (_satsCallback) {
        _satsCallback(_lastSats);
    }
}

int8_t GPS::currentSats_() const {
    if (!_initialized || !_nmea.isValid() || !nmeaFresh()) {
        return -1;
    }

    int sats = (int)_nmea.getNumSatellites();
    if (sats < 0) {
        return -1;
    }
    if (sats > 99) {
        sats = 99;
    }
    return (int8_t)sats;
}

bool GPS::readUtcSignature_(uint64_t &signature) const {
    if (!_initialized || !_nmea.isValid()) {
        return false;
    }

    const uint16_t year = _nmea.getYear();
    const uint8_t month = _nmea.getMonth();
    const uint8_t day = _nmea.getDay();
    const uint8_t hour = _nmea.getHour();
    const uint8_t minute = _nmea.getMinute();
    const uint8_t second = _nmea.getSecond();

    if (year < 2020 || month == 0 || day == 0 ||
        hour > 23 || minute > 59 || second > 60) {
        return false;
    }

    signature = ((uint64_t)year << 26) |
                ((uint64_t)month << 22) |
                ((uint64_t)day << 17) |
                ((uint64_t)hour << 12) |
                ((uint64_t)minute << 6) |
                (uint64_t)second;
    return true;
}

void GPS::updateSats_() {
    int8_t sats = currentSats_();
    if (sats == _lastSats) return;
    _lastSats = sats;
    notifySatsChanged_();
}
