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
// Внимание: GPIO 19/20 заняты USB! Используем 4/5/6
#define GPS_RX_PIN      4
#define GPS_TX_PIN      5
#define GPS_PPS_PIN     6
#define GPS_BAUD        9600

// --- RTC DS3231 (I2C) ---
#define RTC_SDA_PIN     8
#define RTC_SCL_PIN     9
#define RTC_SQW_PIN     7

// --- External Interrupt ---
#define EXT_INT_PIN     15

// --- Serial ---
#define SERIAL_BAUD     115200

// --- BLE ---
#define BLE_DEVICE_NAME "ENTime"

// --- BLE Device Information Service (DIS) ---
#define BLE_DIS_MANUFACTURER  BLE_DEVICE_NAME  // Manufacturer Name
#define BLE_DIS_MODEL         "Gate"           // Model Number
#define BLE_DIS_HARDWARE      "v1"             // Hardware Revision
#define BLE_DIS_FIRMWARE      VERSION          // Firmware version
#define BLE_DIS_SOFTWARE      VERSION          // Software Revision (firmware version)
#define BLE_DIS_SERIAL_PREFIX "FR-"            // Serial Number prefix for auto-generation

// --- SNTP/NTP ---
#define SNTP_SYNC_TIMEOUT_MS    15000
#define SNTP_EDGE_TIMEOUT_MS    2500
#define SNTP_EDGE_WINDOW_US     1500
#define SNTP_POLL_DELAY_MS      50
#define SNTP_EDGE_POLL_DELAY_MS 1

// --- Module Type ---
// Тип модуля устройства
enum class ModuleType {
  START,
  FINISH
};

#endif // CONFIG_H
