#include "main_screen.h"

#include "esp_log.h"

// Глобальный объект главного экрана
MainScreen mainScreen;
static const char* TAG = "MainScreen";

void MainScreen::init(Footer& footer, MainArea& mainArea, StatusBar& statusBar) {
    _footer = &footer;
    _mainArea = &mainArea;
    _statusBar = &statusBar;
}

void MainScreen::draw() {
    if (!_footer || !_mainArea || !_statusBar) {
        ESP_LOGW(TAG, "draw() called before init()");
        return;
    }

    // TODO: реализовать координацию отрисовки status bar / main area / footer.
    (void)_footer;
    (void)_mainArea;
    (void)_statusBar;
}

void MainScreen::update() {
    if (!_footer || !_mainArea || !_statusBar) {
        ESP_LOGW(TAG, "update() called before init()");
        return;
    }

    // TODO: реализовать централизованное обновление главного экрана.
    (void)_footer;
    (void)_mainArea;
    (void)_statusBar;
}
