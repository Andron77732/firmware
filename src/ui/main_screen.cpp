#include "main_screen.h"

#include "esp_log.h"

// Глобальный объект главного экрана
MainScreen mainScreen;
static const char* TAG = "MainScreen";

void MainScreen::init(Footer& footer, MainArea& mainArea, StatusBar& statusBar) {
    this->footer = &footer;
    this->mainArea = &mainArea;
    this->statusBar = &statusBar;
}

void MainScreen::draw() {
    if (!footer || !mainArea || !statusBar) {
        ESP_LOGW(TAG, "draw() called before init()");
        return;
    }

    statusBar->draw();
    mainArea->draw();
    footer->draw();
}

void MainScreen::update() {
    if (!footer || !mainArea || !statusBar) {
        ESP_LOGW(TAG, "update() called before init()");
        return;
    }

    // TODO: реализовать централизованное обновление главного экрана.
    (void)footer;
    (void)mainArea;
    (void)statusBar;
}
