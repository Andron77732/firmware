# ENTime - Проект точного времени по внешнему прерыванию

## 📋 Описание

Устройство синхронизирует время по GPS (резервная копия RTC). По внешнему прерыванию выдаёт точное время **±1мс или лучше**.

**Компоненты:** ESP32-S3, GPS NEO-6M (UART), RTC DS3231 (I2C), TFT ILI9341 (SPI), внешнее прерывание (GPIO)

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
- [ ] PlatformIO setup, зависимости
- [ ] Драйвер NEO-6M (парсинг NMEA)
- [ ] Драйвер DS3231 (I2C)
- [ ] Логирование

### **Фаза 2: Захват времени**
- [ ] GPIO прерывание конфиг
- [ ] **ISR: максимальный приоритет, NO FreeRTOS вызовов**
- [ ] Захват `esp_timer_get_time()` в первой строке ISR
- [ ] Размещение кода ISR в IRAM (минимум jitter)
- [ ] Профилирование latency обработчика
- [ ] Микротаймер ESP32 на 240 MHz
- [ ] Lock-free очередь для буферизации событий

### **Фаза 3: Синхронизация**
- [ ] PPS обработка
- [ ] Синхронизация системного таймера
- [ ] Коррекция RTC дрейфа
- [ ] Дрейф-корректор

### **Фаза 4: Надёжность**
- [ ] GPS → RTC переключение
- [ ] Сохранение времени в NVS
- [ ] Обработка ошибок

### **Фаза 5: Коммуникация**
- [ ] Serial (UART0) команды
- [ ] BluetoothSerial
- [ ] Парсер команд (JSON)

### **Фаза 6: Интерфейс**
- [ ] TFT ILI9341 драйвер
- [ ] UI (время, статус)
- [ ] Мониторинг синхронизации

### **Фаза 7: Тестирование**
- [ ] **[КРИТИЧНО] Измерение latency ISR** (<100µs)
- [ ] **[КРИТИЧНО] Профилирование jitter** (±50µs макс)
- [ ] Unit-тесты (NMEA, команды, time_sync)
- [ ] Integration-тесты (прерывание, переключение)
- [ ] Нагрузочное тестирование (1000+ событий)
- [ ] **Финальная проверка: ±1мс точность**

---

## 📁 Структура кода

```
src/
├── main.cpp
├── hal/
│   ├── gps/              # NEO-6M driver
│   ├── rtc/              # DS3231 driver  
│   ├── tft/              # ILI9341 driver
│   └── comm/             # Serial + Bluetooth
├── timing/               # Time sync + ISR
├── commands/             # Command parser
├── storage/              # Preferences manager
└── config.h

test/
├── test_nmea_parser.cpp
├── test_command_parser.cpp
├── test_time_sync.cpp
└── test_interrupt.cpp
```

---

## 🔧 Интерфейсы

| Интерфейс | Назначение | Пины |
|-----------|-----------|------|
| UART0 | Serial (логирование) | USB встроенный ESP32 |
| UART1 | GPS NEO-6M | RX:44, TX:43, PPS:21 |
| I2C | RTC DS3231 | SDA:8, SCL:9 |
| SPI | TFT ILI9341 | CS:5, DC:6, MOSI:13, MISO:12, SCK:14, RST:15 |
| GPIO | Ext interrupt | PIN:2 |
| BLE | BluetoothSerial | встроенный |

---

## 📚 Зависимости

```ini
lib_deps =
    adafruit/Adafruit DS3231 Library
    adafruit/Adafruit ILI9341
    ArduinoJson
    BluetoothSerial

test_deps = unity
```

---

## 📡 Протокол команд (JSON)

**Запросы:**
```json
{"cmd": "time"}                                  // Текущее время
{"cmd": "status"}                                // Статус синхронизации
{"cmd": "gps", "enable": true}                   // Вкл/выкл GPS
{"cmd": "calibrate", "offset": 0.5}              // RTC offset compensation
{"cmd": "save_config", "data": {...}}            // Сохранить конфиг
{"cmd": "load_config"}                           // Загрузить конфиг
{"cmd": "factory_reset"}                         // Сброс
```

**Ответы:**
```json
{"time": 1703169600123456, "source": "gps", "accuracy_us": 50}
```
