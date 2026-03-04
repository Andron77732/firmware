#include "main_screen.h"

#include "esp_log.h"

static const char* TAG = "MainScreen";

void MainScreen::init(Footer& footer, MainArea& mainArea, StatusBar& statusBar) {
    this->footer = &footer;
    this->mainArea = &mainArea;
    this->statusBar = &statusBar;
    _initialized = true;
}

void MainScreen::draw() {
    if (!_initialized || !footer || !mainArea || !statusBar) {
        ESP_LOGW(TAG, "draw() called before init()");
        return;
    }

    statusBar->draw();
    mainArea->draw();
    footer->draw();
}

void MainScreen::update() {
    if (!_initialized || !footer || !statusBar) {
        return;
    }

    PendingState snapshot;

    portENTER_CRITICAL(&_pendingMux);
    if (_pending.dirtyMask == 0) {
        portEXIT_CRITICAL(&_pendingMux);
        return;
    }
    snapshot = _pending;
    _pending.dirtyMask = 0;
    portEXIT_CRITICAL(&_pendingMux);

    if (snapshot.dirtyMask & DIRTY_BLE) {
        statusBar->updateBluetoothIcon(snapshot.bleState);
    }
    if (snapshot.dirtyMask & DIRTY_WIFI) {
        statusBar->updateWiFiIcon(snapshot.wifiState, snapshot.wifiRssi);
    }
    if (snapshot.dirtyMask & DIRTY_GPS) {
        statusBar->updateGPSIcon(snapshot.gpsState);
    }
    if (snapshot.dirtyMask & DIRTY_TIME_SYNC) {
        footer->updateTimeSyncState(snapshot.timeSyncState);
    }
    if (snapshot.dirtyMask & DIRTY_SATS) {
        footer->updateSats(snapshot.sats);
    }
}

void MainScreen::postBleState(BLEState state) {
    portENTER_CRITICAL(&_pendingMux);
    if (_pending.bleState != state) {
        _pending.bleState = state;
        _pending.dirtyMask |= DIRTY_BLE;
    }
    portEXIT_CRITICAL(&_pendingMux);
}

void MainScreen::postWiFiState(WiFiState state, int8_t rssi) {
    portENTER_CRITICAL(&_pendingMux);
    if (_pending.wifiState != state || _pending.wifiRssi != rssi) {
        _pending.wifiState = state;
        _pending.wifiRssi = rssi;
        _pending.dirtyMask |= DIRTY_WIFI;
    }
    portEXIT_CRITICAL(&_pendingMux);
}

void MainScreen::postGpsState(GPSState state) {
    portENTER_CRITICAL(&_pendingMux);
    if (_pending.gpsState != state) {
        _pending.gpsState = state;
        _pending.dirtyMask |= DIRTY_GPS;
    }
    portEXIT_CRITICAL(&_pendingMux);
}

void MainScreen::postTimeSyncState(TimeSyncState state) {
    portENTER_CRITICAL(&_pendingMux);
    if (_pending.timeSyncState != state) {
        _pending.timeSyncState = state;
        _pending.dirtyMask |= DIRTY_TIME_SYNC;
    }
    portEXIT_CRITICAL(&_pendingMux);
}

void MainScreen::postSats(int8_t sats) {
    portENTER_CRITICAL(&_pendingMux);
    if (_pending.sats != sats) {
        _pending.sats = sats;
        _pending.dirtyMask |= DIRTY_SATS;
    }
    portEXIT_CRITICAL(&_pendingMux);
}
