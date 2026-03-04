#ifndef UI_MAIN_SCREEN_H
#define UI_MAIN_SCREEN_H

#include "footer.h"
#include "main_area.h"
#include "status_bar.h"

/**
 * @brief Координатор главного экрана
 */
class MainScreen {
public:
    /**
     * @brief Инициализация зависимостей главного экрана
     */
    void init(Footer& footer, MainArea& mainArea, StatusBar& statusBar);

    /**
     * @brief Полная отрисовка главного экрана
     */
    void draw();

    /**
     * @brief Обновление состояния главного экрана
     */
    void update();

private:
    Footer* _footer = nullptr;
    MainArea* _mainArea = nullptr;
    StatusBar* _statusBar = nullptr;
};

// Глобальный объект главного экрана
extern MainScreen mainScreen;

#endif // UI_MAIN_SCREEN_H
