# ENTime - Project Plan

Этот файл - актуальный рабочий план и краткая карта проекта. Детальные
спецификации протокола, настроек и алгоритмов живут в `README.md`, `TODO.md` и
`doc/*.md`.

## Описание

ENTime - прошивка для ESP32-S3 старт/финиш модулей точной фиксации событий.
Устройство захватывает внешний GPIO interrupt, привязывает timestamp
`esp_timer_get_time()` к UTC через GPS PPS/NMEA или RTC DS3231 SQW fallback и
отправляет события по Serial/BLE.

Целевая точность timestamp событий - миллисекундный класс или лучше при
валидном GPS PPS/RTC SQW anchor.

Текущая аппаратная база:

- ESP32-S3 DevKitC-1;
- GPS NEO-M8N с PPS;
- RTC DS3231 по I2C + SQW 1 Hz;
- INA226 по I2C для мониторинга питания;
- TFT ILI9341 240x320 SPI с touch через TFT_eSPI;
- внешний вход события GPIO15.

## Архитектура точного времени

Точность достигается не через чтение системных часов в момент события, а через
общую шкалу `esp_timer`:

1. Event ISR фиксирует `esp_timer_get_time()` при внешнем событии.
2. PPS ISR или RTC SQW ISR фиксирует секундный фронт на той же шкале.
3. GPS NMEA, RTC или доверенный holdover дают UTC номер секунды.
4. Событие переводится формулой:

```text
utc_us = anchor_utc_us + (event_esp_us - anchor_esp_us)
```

Подробности: `doc/TIMESYNC.md`.

## Статус фаз

### Фаза 1: Базовая инфраструктура

- [x] PlatformIO setup и зависимости
- [x] GPS NEO-M8N UART + MicroNMEA
- [x] RTC DS3231 I2C
- [x] INA226 I2C
- [x] BLE NUS + Battery + Device Info services
- [x] WiFi STA HAL
- [x] Логирование

### Фаза 2: Захват времени

- [x] GPIO interrupt config
- [x] Event ISR без FreeRTOS вызовов
- [x] Захват `esp_timer_get_time()` в ISR
- [x] IRAM ISR path
- [ ] Профилирование latency обработчика на устройстве

### Фаза 3: Синхронизация

- [x] GPS PPS обработка
- [x] GPS NMEA freshness guards
- [x] GPS PPS/NMEA alignment
- [x] GPS holdover degraded mode
- [x] RTC DS3231 SQW fallback
- [x] RTC lostPower/timeValid guards
- [x] RTC boot clock load для UI
- [x] RTC sync по NTP
- [x] RTC дисциплина от GPS PPS
- [ ] Дальнейшая RTC drift/aging коррекция по полевым измерениям

### Фаза 4: Надёжность

- [x] GPS -> RTC fallback
- [x] RTC-only source policy без GPS holdover
- [x] Command parser oversized-frame discard
- [x] Touch calibration persistence
- [x] Settings write-if-changed для снижения износа NVS
- [ ] Host-side unit tests для ключевой логики
- [ ] Event ISR ring buffer и overflow diagnostics

### Фаза 5: Коммуникация

- [x] Serial JSON commands
- [x] BLE JSON commands через NUS
- [x] BLE event packets
- [x] JSON parser/router/handlers
- [x] WiFi command
- [x] `sync_ntp`, `sync_source`, `calibrate`, `touch_calibrate`
- [x] `gps.enabled` boot-time setting

### Фаза 6: Интерфейс

- [x] TFT ILI9341 UI
- [x] Status bar: time, GPS, BLE, WiFi, battery
- [x] START/FINISH main area
- [x] Boot log screen
- [x] Touch HAL + calibration wizard
- [x] Touch WiFi toggle/status-bar actions
- [ ] Screen lock UX

## Структура кода

```text
src/
├── app/                  # прикладные сценарии: touch actions/calibration
├── command/              # JSON parser, router, command handlers
├── hal/
│   ├── ble/              # BLE + NUS/Battery/DIS services
│   ├── gps/              # NEO-M8N UART + MicroNMEA wrapper
│   ├── ina226/           # power monitor
│   ├── rtc/              # DS3231 wrapper
│   ├── tft/              # TFT wrapper
│   ├── touch/            # touch HAL
│   └── wifi/             # WiFi STA manager
├── runtime/              # build info
├── storage/              # Settings manager
├── timing/               # ISR, event timestamping, PPS/SQW/time sync
└── ui/                   # status bar, footer, main area
```

## План тестирования

Host-side unit tests:

- command parser: valid JSON, invalid JSON, unknown commands, overflow frames;
- settings: validation, defaults, partial updates, factory reset;
- event timestamp formatting and timezone conversion;
- time sync math: PPS/NMEA alignment, stale NMEA, RTC lostPower/timeValid,
  source policy, degraded states.

Device/integration tests:

- event ISR latency under BLE/Serial/WiFi load;
- GPS PPS loss/recovery and RTC fallback;
- RTC SQW loss/warmup holdover;
- BLE notifications and command responses;
- WiFi connect/disconnect and RSSI status;
- touch calibration and UI routing.

## Интерфейсы и пины

| Интерфейс | Назначение | Пины |
|-----------|------------|------|
| UART0 | Serial logs + JSON commands | USB встроенный ESP32 |
| UART2 | GPS NEO-M8N | RX=4, TX=5, PPS=6 |
| I2C | RTC DS3231 + INA226 | SDA=8, SCL=9, SQW=7, INA226 ALERT=16 |
| SPI | TFT ILI9341 + touch | CS=10, DC=18, MOSI=11, MISO=13, SCK=12, RST=14, T_CS=17 |
| GPIO | External event interrupt | GPIO15 |
| BLE | NUS + Battery + Device Info | встроенный |
| WiFi | STA client | встроенный |

Пины задаются в `src/config.h` и `platformio.ini`.

## Зависимости

См. `platformio.ini`:

- `bodmer/TFT_eSPI`
- `stevemarple/MicroNMEA`
- `adafruit/RTClib`
- `robtillaart/INA226`
- `h2zero/NimBLE-Arduino`
- `bblanchon/ArduinoJson`

## JSON-команды

Основная спецификация: `doc/PROTOCOL.md`.

Реализованные основные команды:

- `ping`
- `time`
- `status`
- `wifi`
- `calibrate`
- `sync_source`
- `sync_ntp`
- `save_config`
- `load_config`
- `touch_calibrate`
- `factory_reset`
