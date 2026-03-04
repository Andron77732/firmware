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
    Footer* footer = nullptr;
    MainArea* mainArea = nullptr;
    StatusBar* statusBar = nullptr;

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
};

// Глобальный объект главного экрана
extern MainScreen mainScreen;

#endif // UI_MAIN_SCREEN_H
