#ifndef UI_FOOTER_H
#define UI_FOOTER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "hal/touch/touch.h"
#include "ui_config.h"
#include "timing/time_sync.h"

/**
 * @brief Footer в нижней части экрана
 * 
 * Layout (240px width, 12px height):
 * ┌────────────────────────────────────────┐
 * │ START v0.1.0                           │
 * └────────────────────────────────────────┘
 * 
 * Отображает тип модуля и версию прошивки слева
 */
class Footer {
public:

    /**
     * @brief Инициализация footer
     * @param tft Ссылка на TFT объект
     * @param moduleType Тип модуля (START/FINISH)
     * @param version Версия прошивки (например, "0.1.0")
     */
    void init(TFT_eSPI& tft, ModuleType moduleType, const String& version);
    
    /**
     * @brief Полная отрисовка footer (фон + текст)
     */
    void draw();

    /**
     * @brief Обновление количества спутников
     * @param sats Количество спутников, -1 если недоступно
     */
    void updateSats(int8_t sats);

    /**
     * @brief Обновление состояния синхронизации времени
     * @param state Состояние синхронизации
     */
    void updateTimeSyncState(TimeSyncState state);

    /**
     * @brief Обработчик touch-событий
     * @return true если событие обработано
     */
    bool onTouchEvent(const TouchEvent& event);

private:
    TFT_eSPI* _tft = nullptr;
    ModuleType _moduleType = ModuleType::START;
    String _version;
    TimeSyncState _state = TimeSyncState::NONE;
    int8_t _sats = -1;

    void drawSatsValue(int8_t sats, TimeSyncState state);
    void drawTimeSyncValue(TimeSyncState state, int8_t sats);
};

// Глобальный объект footer
extern Footer footer;

#endif // UI_FOOTER_H
