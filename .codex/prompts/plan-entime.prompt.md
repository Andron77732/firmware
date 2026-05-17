# ENTime - Проект точного времени по внешнему прерыванию

## 📋 Описание

Устройство синхронизирует время по GPS (резервная копия RTC). По внешнему прерыванию выдаёт точное время **±1мс или лучше**.

**Компоненты:** ESP2-S3-devkitc-1, GPS NEO-6M (UART), RTC DS3231 (I2C), INA219 (I2C), 3.2" TFT 240*320 ILI9341 (SPI), внешнее прерывание (GPIO)

---

## 🏗️ Архитектура

**Точность достигается:**
1. ISR захватывает `esp_timer_get_time()` при прерывании (микросекундная точность)
2. Синхронизация по GPS PPS сигналу
3. Интерполяция: GPS_time + timer_diff
4. Резервная синхронизация по RTC DS3231

---

## 📅 Фазы разработки

### **Фаза 1: Базовая инфраструктура**
- [x] PlatformIO setup, зависимости
- [x] Инициализация NEO-6M (парсинг NMEA)
- [x] Инициализация DS3231 (I2C)
- [x] Инициализация BLE (GATT)
- [x] Логирование

### **Фаза 2: Захват времени**
- [x] GPIO прерывание конфиг
- [x] **ISR: максимальный приоритет, NO FreeRTOS вызовов**
- [x] Захват `esp_timer_get_time()` в первой строке ISR
- [x] Размещение кода ISR в IRAM (минимум jitter)
- [ ] Профилирование latency обработчика
- [ ] Микротаймер ESP32 на 240 MHz

### **Фаза 3: Синхронизация**
- [x] PPS обработка
- [x] Синхронизация системного таймера
- [ ] Коррекция RTC дрейфа
- [ ] Дрейф-корректор

### **Фаза 4: Надёжность**
- [x] GPS → RTC переключение
- [ ] Сохранение времени в NVS
- [ ] Обработка ошибок

### **Фаза 5: Коммуникация**
- [ ] Serial (UART0) команды
- [x] BLE (Nordic UART Service)
- [ ] WiFi STA (клиент): включение/выключение, подключение к SSID
- [ ] Парсер команд (JSON)

### **Фаза 6: Интерфейс**
- [x] TFT ILI9341 драйвер
- [ ] UI (время, статус)
- [ ] Мониторинг синхронизации

---

## 📁 Структура кода

```
src/
├── main.cpp
├── config.h
├── command/              # Router + parser + handlers
├── hal/
│   ├── ble/              # BLE + battery/device info
│   ├── wifi/             # WiFi STA (client)
│   ├── gps/              # NEO-6M driver
│   ├── rtc/              # DS3231 driver
│   ├── tft/              # ILI9341 driver
│   └── power/            # INA219 driver
├── storage/              # Settings manager
├── timing/               # Time sync + ISR
└── ui/                   # UI components (status bar, footer, main area)

test/
```

---

## ✅ План тестирования

**Unit (host-side):**
- Парсер команд: корректный/битый JSON, неизвестные команды, граничные значения
- Парсер NMEA: валидные/битые предложения, отсутствие PPS, разрывы по времени
- Математика синхронизации: расчёт смещения, обработка jitter, GPS→RTC fallback
- Логика RTC: lostPower, некорректное время, перевод часового пояса
- Таймстемпы событий: последовательность START/FINISH и форматирование
- Логика UI: соответствие иконок состояниям, формат времени (mock display)
- Settings: валидация, дефолты, сценарии load/reset

**Integration (device):**
- Логирование latency PPS ISR под нагрузкой
- Переключение RTC + GPS и восстановление
- BLE уведомления состояния батареи/сервиса
- WiFi connect/disconnect и уровни RSSI

---

## 🔧 Интерфейсы

| Интерфейс | Назначение | Пины |
|-----------|-----------|------|
| UART0 | Serial (логирование) | USB встроенный ESP32 |
| UART2 | GPS NEO-6M | RX:4, TX:5, PPS:6 |
| I2C | RTC DS3231 + INA219 | SDA:8, SCL:9, SQW:7 |
| SPI | TFT ILI9341 | CS:10, DC:18, MOSI:11, MISO:13, SCK:12, RST:14, T_CS:17 |
| GPIO | Ext interrupt | PIN:3 |
| BLE | GATT | встроенный |
| WiFi | STA (клиент) | встроенный |

---

## 📚 Зависимости

```ini
lib_deps =
    adafruit/Adafruit DS3231 Library
    bodmer/TFT_eSPI
    h2zero/NimBLE-Arduino
    stevemarple/MicroNMEA
```

---

## 📡 Протокол команд (JSON)

**Запросы:**
```json
{"cmd": "time"}                                  // Текущее время
{"cmd": "status"}                                // Статус синхронизации
{"cmd": "gps", "enable": true}                   // Вкл/выкл GPS
{"cmd": "wifi", "enable": true}                  // Вкл/выкл WiFi
{"cmd": "wifi", "connect": {"ssid": "Network", "password": "pass"}}  // Подключение к WiFi
{"cmd": "calibrate", "offset": 0.5}              // RTC offset compensation
{"cmd": "save_config", "data": {...}}            // Сохранить конфиг
{"cmd": "load_config"}                           // Загрузить конфиг
{"cmd": "factory_reset"}                         // Сброс
```

**Ответы:**
```json
{"time": 1703169600123456, "source": "gps", "accuracy_us": 50}
```
