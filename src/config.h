#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// ENTime Firmware Configuration
// ============================================================================

#define VERSION "0.1.0"

// --- TFT Display (ILI9341) ---
// Пины определены в platformio.ini через build_flags для TFT_eSPI
// TFT_CS=10, TFT_DC=18, TFT_RST=14
// TFT_MOSI=11, TFT_MISO=13, TFT_SCLK=12
// TOUCH_CS=17

// --- GPS NEO-6M (UART1) ---
#define GPS_RX_PIN      44
#define GPS_TX_PIN      43
#define GPS_PPS_PIN     21

// --- RTC DS3231 (I2C) ---
#define RTC_SDA_PIN     8
#define RTC_SCL_PIN     9

// --- External Interrupt ---
#define EXT_INT_PIN     2

// --- Serial ---
#define SERIAL_BAUD     115200

#endif // CONFIG_H

