# ENTime - Протокол команд

Все команды передаются в формате JSON через Serial (UART0) или BluetoothSerial. Ответы также в JSON.

---

## 📤 Команды (Request)

### `time` - Получить текущее время

**Запрос:**
```json
{"cmd": "time"}
```

**Ответ:**
```json
{
  "cmd": "time",
  "time": 1703169600123456,
  "source": "gps",
  "accuracy_us": 50,
  "status": "ok"
}
```

**Параметры ответа:**
- `time` — текущее время в микросекундах с UNIX epoch
- `source` — источник времени: `"gps"` или `"rtc"`
- `accuracy_us` — точность в микросекундах (±)
- `status` — `"ok"`, `"warning"`, `"error"`

---

### `status` - Получить статус синхронизации

**Запрос:**
```json
{"cmd": "status"}
```

**Ответ:**
```json
{
  "cmd": "status",
  "gps_fix": true,
  "gps_satellites": 12,
  "pps_signal": true,
  "rtc_drifting": false,
  "last_sync": 5000,
  "sync_source": "gps",
  "battery_voltage": 5.0,
  "temperature_c": 24.5,
  "status": "ok"
}
```

**Параметры:**
- `gps_fix` — есть ли GPS фиксация
- `gps_satellites` — количество видимых спутников
- `pps_signal` — получен ли PPS сигнал от GPS
- `rtc_drifting` — дрейф RTC (true если >100ppm)
- `last_sync` — время последней синхронизации (мс)
- `sync_source` — текущий источник: `"gps"` или `"rtc"`
- `battery_voltage` — напряжение питания
- `temperature_c` — температура датчика RTC

---

### `gps` - Управление GPS модулем

**Запрос (включить):**
```json
{"cmd": "gps", "enable": true}
```

**Запрос (отключить):**
```json
{"cmd": "gps", "enable": false}
```

**Ответ:**
```json
{
  "cmd": "gps",
  "state": "enabled",
  "power_consumption_ma": 45,
  "status": "ok"
}
```

---

### `calibrate` - Калибровка RTC

**Запрос:**
```json
{"cmd": "calibrate", "offset": 0.5}
```

**Параметры:**
- `offset` — смещение в ppm (part per million) для коррекции RTC

**Ответ:**
```json
{
  "cmd": "calibrate",
  "previous_offset": 0.2,
  "new_offset": 0.5,
  "estimated_error_us": 10,
  "status": "ok"
}
```

---

### `sync_source` - Переключить источник синхронизации

**Запрос (на GPS):**
```json
{"cmd": "sync_source", "source": "gps"}
```

**Запрос (на RTC):**
```json
{"cmd": "sync_source", "source": "rtc"}
```

**Ответ:**
```json
{
  "cmd": "sync_source",
  "active_source": "rtc",
  "reason": "no_gps_signal",
  "timestamp": 1703169600123456,
  "status": "ok"
}
```

---

### `sync_ntp` - Синхронизация RTC по NTP

**Запрос:**
```json
{"cmd": "sync_ntp"}
```

**Ответ (успех):**
```json
{
  "cmd": "sync_ntp",
  "status": "ok",
  "rtc_time": 1703169600,
  "ntp_server": "ru.pool.ntp.org",
  "sync_duration_ms": 2500
}
```

**Ответ (ошибка):**
```json
{
  "cmd": "sync_ntp",
  "status": "error",
  "error_code": 203,
  "error_message": "WiFi not connected"
}
```

**Параметры ответа:**
- `status` — `"ok"`, `"warning"`, `"error"`
- `rtc_time` — время, записанное в RTC (UNIX timestamp в секундах, UTC)
- `ntp_server` — использованный NTP сервер
- `sync_duration_ms` — длительность синхронизации в миллисекундах
- `error_code` — код ошибки (при status="error")
- `error_message` — описание ошибки

**Требования:**
- WiFi должен быть подключён
- RTC должен быть инициализирован

**Примечание:** Команда выполняет точную синхронизацию RTC по NTP с выравниванием на границу секунды. Используются серверы из конфигурации (SNTP_SERVER_1, SNTP_SERVER_2).

---

### `save_config` - Сохранить конфигурацию

**Запрос:**
```json
{
  "cmd": "save_config",
  "data": {
    "device": {
      "name": "ENTime-Lab",
      "number": 1,
      "type": 1,
      "timezone": 3
    },
    "sync": {
      "auto": true,
      "source": 0
    },
    "wifi": {
      "active": false,
      "ssid": "",
      "passwd": ""
    },
    "calibration": {
      "rtc_offset_ppm": 0.5
    }
  }
}
```

**Ответ:**
```json
{
  "cmd": "save_config",
  "saved_keys": 5,
  "storage_usage_percent": 15,
  "status": "ok"
}
```

**Примечание:** При сохранении можно указывать только нужные группы или параметры внутри групп. Остальные останутся без изменений. См. [SETTINGS.md](SETTINGS.md) для подробного описания всех настроек.

**Атомарность:** Операция `save_config` является атомарной - все указанные группы и параметры валидируются перед применением. Если хотя бы одна группа не прошла валидацию, изменения не применяются и возвращается ошибка. Это гарантирует, что либо все изменения применяются успешно, либо настройки остаются без изменений.

---

### `load_config` - Загрузить конфигурацию

**Запрос:**
```json
{"cmd": "load_config"}
```

**Ответ:**
```json
{
  "cmd": "load_config",
  "data": {
    "device": {
      "name": "ENTime-Lab",
      "number": 1,
      "type": 1,
      "timezone": 3
    },
    "sync": {
      "auto": true,
      "source": 0
    },
    "wifi": {
      "active": false,
      "ssid": "",
      "passwd": ""
    },
    "calibration": {
      "rtc_offset_ppm": 0.5
    }
  },
  "status": "ok"
}
```

**Примечание:** Все настройки организованы в иерархическую структуру по функциональным категориям. См. [SETTINGS.md](SETTINGS.md) для подробного описания всех параметров.

---

### `factory_reset` - Сброс к заводским настройкам

**Запрос:**
```json
{"cmd": "factory_reset"}
```

**Ответ:**
```json
{
  "cmd": "factory_reset",
  "message": "Device will reset in 2 seconds",
  "status": "ok"
}
```

---

## 📥 Ответы (Response)

### Общая структура ответа

```json
{
  "cmd": "command_name",
  "status": "ok|warning|error",
  "error_code": 0,
  "error_message": "",
  "timestamp": 1703169600123456,
  "...": "command_specific_fields"
}
```

### Коды ошибок

| Код | Статус | Описание |
|-----|--------|---------|
| 0 | ok | Успешно |
| 1 | warning | Предупреждение, команда выполнена частично |
| 100 | error | Неизвестная команда |
| 101 | error | Неверный формат JSON |
| 102 | error | Отсутствует обязательный параметр |
| 103 | error | Недопустимое значение параметра |
| 200 | error | GPS не инициализирован |
| 201 | error | RTC не инициализирован |
| 202 | error | Ошибка сохранения в Preferences |
| 203 | error | Ошибка при синхронизации времени |

---

## 📋 Примеры сценариев

### Сценарий 1: Запрос текущего времени с проверкой точности

```json
// Запрос
{"cmd": "time"}

// Ответ
{
  "cmd": "time",
  "time": 1703169600123456,
  "source": "gps",
  "accuracy_us": 50,
  "status": "ok"
}
```

### Сценарий 2: Проверка статуса и калибровка если нужна

```json
// 1. Запрос статуса
{"cmd": "status"}

// Ответ: RTC дрейфит
{
  "sync_source": "rtc",
  "rtc_drifting": true,
  "status": "warning"
}

// 2. Калибровка
{"cmd": "calibrate", "offset": 0.8}

// Ответ
{
  "previous_offset": 0.2,
  "new_offset": 0.8,
  "status": "ok"
}
```

### Сценарий 3: Потеря GPS, fallback на RTC

```json
// Статус: GPS сигнал потерян
{
  "gps_fix": false,
  "sync_source": "rtc",
  "reason": "no_gps_signal",
  "status": "warning"
}

// При восстановлении GPS
{
  "gps_fix": true,
  "pps_signal": true,
  "sync_source": "gps",
  "status": "ok"
}
```

---

## 🔄 Асинхронные события

Устройство может отправлять асинхронные уведомления (без запроса):

```json
// Потеря GPS
{"event": "gps_lost", "timestamp": 1703169600123456}

// Восстановление GPS
{"event": "gps_acquired", "satellites": 12, "timestamp": 1703169600123456}

// Ошибка RTC
{"event": "rtc_error", "error_code": 201, "timestamp": 1703169600123456}

// Переключение источника синхронизации
{"event": "sync_source_changed", "new_source": "rtc", "reason": "gps_timeout"}
```

---

## 🚀 Рекомендации по интеграции

1. **Timeouts:** Ждите ответ максимум 100мс
2. **Retry logic:** При timeout повторите команду до 3 раз
3. **Queue commands:** Отправляйте по одной команде за раз, ждите ответа
4. **Monitor events:** Слушайте асинхронные события для быстрого реагирования
5. **Validate JSON:** Используйте JSON schema для валидации
