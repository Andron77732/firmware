#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <Arduino.h>
#include <TFT_eSPI.h>

/**
 * @file ui_config.h
 * @brief Централизованные настройки для всех UI компонентов
 */

// ============================================================================
// StatusBar Settings
// ============================================================================

// Размеры и позиции
#define UI_STATUS_BAR_WIDTH  240
#define UI_STATUS_BAR_HEIGHT 24   // 4 + 16 + 4
#define UI_STATUS_BAR_Y_POS  0

// Зона часов (TextSize 2 = 16px height)
#define UI_STATUS_BAR_CLOCK_X        4
#define UI_STATUS_BAR_CLOCK_Y        4
#define UI_STATUS_BAR_CLOCK_TEXT_SIZE 2   // 12x16 per char, "HH:MM:SS" = 96px
#define UI_STATUS_BAR_CLOCK_HEIGHT   16   // 8 * CLOCK_TEXT_SIZE

// Зона иконок (справа налево, от правого края)
#define UI_STATUS_BAR_ICON_SIZE   16   // 16px — равно высоте часов
#define UI_STATUS_BAR_ICON_PADDING 4
#define UI_STATUS_BAR_ICON_Y       4    // выровнено с часами

// Позиции иконок (X координата левого края, справа налево)
#define UI_STATUS_BAR_ICON_BATTERY_X   220  // WIDTH - ICON_PADDING - ICON_SIZE
#define UI_STATUS_BAR_ICON_WIFI_X      200  // ICON_BATTERY_X - ICON_PADDING - ICON_SIZE
#define UI_STATUS_BAR_ICON_BLUETOOTH_X 180  // ICON_WIFI_X - ICON_PADDING - ICON_SIZE
#define UI_STATUS_BAR_ICON_GPS_X       160  // ICON_BLUETOOTH_X - ICON_PADDING - ICON_SIZE

// Цвета
#define UI_STATUS_BAR_COLOR_BACKGROUND          TFT_BLACK
#define UI_STATUS_BAR_COLOR_CLOCK               TFT_WHITE
#define UI_STATUS_BAR_COLOR_ICON_ACTIVE         TFT_WHITE
#define UI_STATUS_BAR_COLOR_ICON_INACTIVE       TFT_DARKGREY
#define UI_STATUS_BAR_COLOR_ICON_BLUETOOTH_ACTIVE TFT_BLUE

// Пороговые значения RSSI для определения уровня сигнала WiFi (в dBm)
// Уровень 4 (отличный): > UI_WIFI_RSSI_LEVEL_4
// Уровень 3 (хороший):  > UI_WIFI_RSSI_LEVEL_3
// Уровень 2 (средний):  > UI_WIFI_RSSI_LEVEL_2
// Уровень 1 (слабый):   > UI_WIFI_RSSI_LEVEL_1
// Уровень 0 (очень слабый): <= UI_WIFI_RSSI_LEVEL_1
#define UI_WIFI_RSSI_LEVEL_4  -50  // Отличный сигнал
#define UI_WIFI_RSSI_LEVEL_3  -60  // Хороший сигнал
#define UI_WIFI_RSSI_LEVEL_2  -70  // Средний сигнал
#define UI_WIFI_RSSI_LEVEL_1  -80  // Слабый сигнал

// ============================================================================
// MainArea Settings
// ============================================================================

// Размеры и позиции
#define UI_MAIN_AREA_WIDTH  240
#define UI_MAIN_AREA_HEIGHT 284  // 320 - 24 - 12
#define UI_MAIN_AREA_Y_POS  24   // После status_bar

// Цвета
#define UI_MAIN_AREA_COLOR_BACKGROUND TFT_BLACK

// Параметры отображения логов
#define UI_MAIN_AREA_LOG_X            4
#define UI_MAIN_AREA_LOG_Y            4
#define UI_MAIN_AREA_LOG_TEXT_SIZE    1   // 8px height per line
#define UI_MAIN_AREA_LOG_LINE_SPACING 4   // Дополнительный отступ между строками (px)
#define UI_MAIN_AREA_LOG_PADDING_RIGHT 4  // Отступ справа для безопасности

// Параметры шрифта (для размера 1 в TFT_eSPI: 6px ширина, 8px высота)
#define UI_MAIN_AREA_LOG_CHAR_HEIGHT 8   // Ширина одного символа в пикселях для размера 1
#define UI_MAIN_AREA_LOG_CHAR_WIDTH  6   // Ширина одного символа в пикселях для размера 1

// Вычисляемые параметры логов
#define UI_MAIN_AREA_LOG_TEXT_HEIGHT       (UI_MAIN_AREA_LOG_CHAR_HEIGHT * UI_MAIN_AREA_LOG_TEXT_SIZE)  // Высота текста одной строки
#define UI_MAIN_AREA_LOG_LINE_HEIGHT       (UI_MAIN_AREA_LOG_TEXT_HEIGHT + UI_MAIN_AREA_LOG_LINE_SPACING)  // Полная высота строки с отступом
#define UI_MAIN_AREA_LOG_AVAILABLE_HEIGHT  (UI_MAIN_AREA_HEIGHT - UI_MAIN_AREA_LOG_Y)  // Доступная высота для логов
#define UI_MAIN_AREA_MAX_LOG_LINES         (UI_MAIN_AREA_LOG_AVAILABLE_HEIGHT / UI_MAIN_AREA_LOG_LINE_HEIGHT)  // Максимальное количество строк
#define UI_MAIN_AREA_LOG_AVAILABLE_WIDTH   (UI_MAIN_AREA_WIDTH - UI_MAIN_AREA_LOG_X - UI_MAIN_AREA_LOG_PADDING_RIGHT)  // Доступная ширина для логов
#define UI_MAIN_AREA_LOG_LINE_LENGTH       ((UI_MAIN_AREA_LOG_AVAILABLE_WIDTH / UI_MAIN_AREA_LOG_CHAR_WIDTH) + 1)  // Максимальная длина строки (+1 для '\0')

// Цвета логов
#define UI_MAIN_AREA_LOG_COLOR        TFT_WHITE
#define UI_MAIN_AREA_LOG_COLOR_ERROR  TFT_RED
#define UI_MAIN_AREA_LOG_COLOR_WARNING TFT_YELLOW

// ============================================================================
// Footer Settings
// ============================================================================

// Размеры и позиции
#define UI_FOOTER_WIDTH  240
#define UI_FOOTER_HEIGHT 12
#define UI_FOOTER_Y_POS  308  // 320 - 12

// Позиция текста
#define UI_FOOTER_TEXT_X    4
#define UI_FOOTER_TEXT_Y    2   // Центрирование: (12-8)/2 = 2
#define UI_FOOTER_TEXT_SIZE 1   // 8px height per line

// Цвета
#define UI_FOOTER_COLOR_BACKGROUND TFT_BLACK
#define UI_FOOTER_COLOR_TEXT       TFT_WHITE

#endif // UI_CONFIG_H

