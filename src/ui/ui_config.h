#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <Arduino.h>
#include <TFT_eSPI.h>

/**
 * @file ui_config.h
 * @brief Централизованные настройки для всех UI компонентов
 */

// ============================================================================
// Screen Settings
// ============================================================================

#define UI_SCREEN_HEIGHT 320
#define UI_SCREEN_WIDTH  240

// Параметры шрифта (для размера 1 в TFT_eSPI: 6px ширина, 8px высота)
#define UI_CHAR_HEIGHT 8   // Высота одного символа в пикселях для размера 1
#define UI_CHAR_WIDTH  6   // Ширина одного символа в пикселях для размера 1

// ============================================================================
// StatusBar Settings
// ============================================================================

// Зона часов (TextSize 2 = 16px height)
#define UI_STATUS_BAR_CLOCK_X          UI_STATUS_BAR_PADDING
#define UI_STATUS_BAR_CLOCK_Y          UI_STATUS_BAR_PADDING
#define UI_STATUS_BAR_CLOCK_CHAR_COUNT 8   // "HH:MM:SS"
#define UI_STATUS_BAR_CLOCK_TEXT_SIZE  2   // 12x16 per char, "HH:MM:SS" = 96px
#define UI_STATUS_BAR_CLOCK_WIDTH      (UI_STATUS_BAR_CLOCK_CHAR_COUNT * UI_CHAR_WIDTH * UI_STATUS_BAR_CLOCK_TEXT_SIZE)
#define UI_STATUS_BAR_CLOCK_HEIGHT     (UI_CHAR_HEIGHT * UI_STATUS_BAR_CLOCK_TEXT_SIZE)   // 8 * CLOCK_TEXT_SIZE

// Размеры и позиции
#define UI_STATUS_BAR_WIDTH  UI_SCREEN_WIDTH
#define UI_STATUS_BAR_PADDING 4
#define UI_STATUS_BAR_HEIGHT  (UI_STATUS_BAR_CLOCK_HEIGHT + UI_STATUS_BAR_PADDING * 2)   // 4 + 16 + 4
#define UI_STATUS_BAR_Y_POS   0

// Зона иконок (справа налево, от правого края)
#define UI_STATUS_BAR_ICON_SIZE    UI_STATUS_BAR_CLOCK_HEIGHT // 16px — равно высоте часов
#define UI_STATUS_BAR_ICON_PADDING UI_STATUS_BAR_PADDING
#define UI_STATUS_BAR_ICON_Y       UI_STATUS_BAR_PADDING    // выровнено с часами

// Позиции иконок (X координата левого края, справа налево)
#define UI_STATUS_BAR_ICON_BATTERY_X   (UI_SCREEN_WIDTH - UI_STATUS_BAR_ICON_PADDING - UI_STATUS_BAR_ICON_SIZE)
#define UI_STATUS_BAR_ICON_WIFI_X      (UI_STATUS_BAR_ICON_BATTERY_X - UI_STATUS_BAR_ICON_PADDING - UI_STATUS_BAR_ICON_SIZE)
#define UI_STATUS_BAR_ICON_BLUETOOTH_X (UI_STATUS_BAR_ICON_WIFI_X - UI_STATUS_BAR_ICON_PADDING - UI_STATUS_BAR_ICON_SIZE)
#define UI_STATUS_BAR_ICON_GPS_X       (UI_STATUS_BAR_ICON_BLUETOOTH_X - UI_STATUS_BAR_ICON_PADDING - UI_STATUS_BAR_ICON_SIZE)

// Цвета
#define TFT_RAISIN_BLACK 0x2104
#define TFT_JET 0x31A6 
#define UI_STATUS_BAR_COLOR_BACKGROUND              TFT_BLACK
#define UI_STATUS_BAR_COLOR_CLOCK                   TFT_WHITE
#define UI_STATUS_BAR_COLOR_CLOCK_NO_SYNC           TFT_RED
#define UI_STATUS_BAR_COLOR_ICON_ACTIVE             TFT_WHITE
#define UI_STATUS_BAR_COLOR_ICON_INACTIVE           TFT_RAISIN_BLACK
#define UI_STATUS_BAR_COLOR_ICON_WIFI_UNINITIALIZED UI_STATUS_BAR_COLOR_ICON_INACTIVE
#define UI_STATUS_BAR_COLOR_ICON_WIFI_OFF           UI_STATUS_BAR_COLOR_ICON_INACTIVE
#define UI_STATUS_BAR_COLOR_ICON_WIFI_CONNECTING    TFT_YELLOW
#define UI_STATUS_BAR_COLOR_ICON_WIFI_CONNECTED     TFT_GREEN
#define UI_STATUS_BAR_COLOR_ICON_WIFI_DISCONNECTED  UI_STATUS_BAR_COLOR_ICON_ACTIVE
#define UI_STATUS_BAR_COLOR_ICON_WIFI_RECONNECTING  TFT_YELLOW
#define UI_STATUS_BAR_COLOR_ICON_WIFI_ERROR         TFT_YELLOW
#define UI_STATUS_BAR_COLOR_ICON_BLUETOOTH_ACTIVE   TFT_BLUE
#define UI_STATUS_BAR_COLOR_ICON_GPS_ACTIVE         TFT_GREEN
#define UI_STATUS_BAR_COLOR_ICON_GPS_SEARCHING      TFT_YELLOW
#define UI_STATUS_BAR_COLOR_ICON_BATTERY_CHARGING   TFT_GREEN

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
#define UI_MAIN_AREA_WIDTH  UI_SCREEN_WIDTH
#define UI_MAIN_AREA_HEIGHT (UI_SCREEN_HEIGHT - UI_STATUS_BAR_HEIGHT - UI_FOOTER_HEIGHT)  // 320 - 24 - 12
#define UI_MAIN_AREA_Y_POS  UI_STATUS_BAR_HEIGHT                                          // После status_bar

// Цвета
#define UI_MAIN_AREA_COLOR_BACKGROUND TFT_BLACK

// Параметры отображения логов
#define UI_MAIN_AREA_LOG_X             4
#define UI_MAIN_AREA_LOG_Y             4
#define UI_MAIN_AREA_LOG_TEXT_SIZE     1   // 8px height per line
#define UI_MAIN_AREA_LOG_LINE_SPACING  4   // Дополнительный отступ между строками (px)
#define UI_MAIN_AREA_LOG_PADDING_RIGHT 4   // Отступ справа для безопасности

// Вычисляемые параметры логов
#define UI_MAIN_AREA_LOG_TEXT_HEIGHT       (UI_CHAR_HEIGHT * UI_MAIN_AREA_LOG_TEXT_SIZE)  // Высота текста одной строки
#define UI_MAIN_AREA_LOG_LINE_HEIGHT       (UI_MAIN_AREA_LOG_TEXT_HEIGHT + UI_MAIN_AREA_LOG_LINE_SPACING)  // Полная высота строки с отступом
#define UI_MAIN_AREA_LOG_AVAILABLE_HEIGHT  (UI_MAIN_AREA_HEIGHT - UI_MAIN_AREA_LOG_Y)  // Доступная высота для логов
#define UI_MAIN_AREA_MAX_LOG_LINES         (UI_MAIN_AREA_LOG_AVAILABLE_HEIGHT / UI_MAIN_AREA_LOG_LINE_HEIGHT)  // Максимальное количество строк
#define UI_MAIN_AREA_LOG_HISTORY_LINES     128  // Сколько строк LOADING-лога хранить для прокрутки
#define UI_MAIN_AREA_LOG_AVAILABLE_WIDTH   (UI_MAIN_AREA_WIDTH - UI_MAIN_AREA_LOG_X - UI_MAIN_AREA_LOG_PADDING_RIGHT)  // Доступная ширина для логов
#define UI_MAIN_AREA_LOG_LINE_LENGTH       ((UI_MAIN_AREA_LOG_AVAILABLE_WIDTH / UI_CHAR_WIDTH) + 1)  // Максимальная длина строки (+1 для '\0')

// Цвета логов
#define UI_MAIN_AREA_LOG_COLOR        TFT_WHITE
#define UI_MAIN_AREA_LOG_COLOR_ERROR  TFT_RED
#define UI_MAIN_AREA_LOG_COLOR_WARNING TFT_YELLOW

// Параметры отображения финишного main_area
#define UI_MAIN_AREA_FINISH_TIME_TEXT_SIZE 3
#define UI_MAIN_AREA_FINISH_TIME_HEIGHT    (UI_CHAR_HEIGHT * UI_MAIN_AREA_FINISH_TIME_TEXT_SIZE)
#define UI_MAIN_AREA_FINISH_TIME_X         12
#define UI_MAIN_AREA_FINISH_TIME_Y         8
#define UI_MAIN_AREA_FINISH_TIME_SPACING   10
#define UI_MAIN_AREA_FINISH_GAP_SPACER_MS  2000  // Минимальный разрыв между финишами для пустой строки в списке
#define UI_MAIN_AREA_FINISH_LOG_Y          (UI_MAIN_AREA_FINISH_TIME_Y + UI_MAIN_AREA_FINISH_TIME_HEIGHT + UI_MAIN_AREA_FINISH_TIME_SPACING)
#define UI_MAIN_AREA_FINISH_AVAILABLE_HEIGHT (UI_MAIN_AREA_HEIGHT - UI_MAIN_AREA_FINISH_LOG_Y)
#define UI_MAIN_AREA_FINISH_MAX_LOG_LINES  (UI_MAIN_AREA_FINISH_AVAILABLE_HEIGHT / UI_MAIN_AREA_LOG_LINE_HEIGHT)

// Параметры отображения стартового main_area
#define UI_MAIN_AREA_START_COUNTDOWN_START 55
#define UI_MAIN_AREA_START_COUNTDOWN_TEXT_SIZE 8
#define UI_MAIN_AREA_START_COUNTDOWN_HEIGHT (UI_CHAR_HEIGHT * UI_MAIN_AREA_START_COUNTDOWN_TEXT_SIZE)
#define UI_MAIN_AREA_START_COUNTDOWN_Y 12
#define UI_MAIN_AREA_START_COUNTDOWN_SPACING 12

#define UI_MAIN_AREA_START_CORRECTION_TEXT_SIZE 4
#define UI_MAIN_AREA_START_CORRECTION_HEIGHT (UI_CHAR_HEIGHT * UI_MAIN_AREA_START_CORRECTION_TEXT_SIZE)
#define UI_MAIN_AREA_START_CORRECTION_PADDING 4
#define UI_MAIN_AREA_START_CORRECTION_TEXT_Y_OFFSET 2
#define UI_MAIN_AREA_START_CORRECTION_SPACING 8
#define UI_MAIN_AREA_START_CORRECTION_COLOR_BACKGROUND TFT_BLUE
#define UI_MAIN_AREA_START_CORRECTION_COLOR_TEXT TFT_WHITE

#define UI_MAIN_AREA_START_LIST_SPACING 6

// Параметры отображения экрана питания
#define UI_MAIN_AREA_POWER_TITLE_X      4
#define UI_MAIN_AREA_POWER_TITLE_Y      8
#define UI_MAIN_AREA_POWER_TITLE_SIZE   2
#define UI_MAIN_AREA_POWER_ROW_X        4
#define UI_MAIN_AREA_POWER_ROW_Y        40
#define UI_MAIN_AREA_POWER_ROW_HEIGHT   24
#define UI_MAIN_AREA_POWER_TEXT_SIZE    2

// ============================================================================
// Footer Settings
// ============================================================================

// Позиция текста
#define UI_FOOTER_PADDING   2
#define UI_FOOTER_TEXT_X    (UI_FOOTER_PADDING * 2)
#define UI_FOOTER_TEXT_SIZE 1   // 8px height per line

// Размеры и позиции
#define UI_FOOTER_WIDTH  UI_SCREEN_WIDTH
#define UI_FOOTER_HEIGHT (UI_CHAR_HEIGHT * UI_FOOTER_TEXT_SIZE + UI_FOOTER_PADDING * 2)  // 8 + 4
#define UI_FOOTER_Y_POS  (UI_SCREEN_HEIGHT - UI_FOOTER_HEIGHT)  // 320 - 12

// Цвета
#define UI_FOOTER_COLOR_BACKGROUND TFT_BLACK
#define UI_FOOTER_COLOR_TEXT       TFT_WHITE

#endif // UI_CONFIG_H
