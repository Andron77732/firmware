# TODO - ENTime Firmware

Список задач для разработки проекта ENTime. Основной план проекта: [.cursor/prompts/plan-entime.prompt.md](.cursor/prompts/plan-entime.prompt.md)

---

## Фаза 3: Синхронизация

- [x] Синхронизация RTC по NTP

---

## Фаза 5: Коммуникация

- [ ] Serial (UART0) команды
  - [x] [ping](PROTOCOL.md#ping---проверка-связи)
  - [x] [time](PROTOCOL.md#time---получить-текущее-время)
  - [ ] [status](PROTOCOL.md#status---получить-статус-синхронизации)
  - [ ] [gps](PROTOCOL.md#gps---управление-gps-модулем)
  - [x] [wifi](PROTOCOL.md#wifi---управление-wifi)
  - [ ] [calibrate](PROTOCOL.md#calibrate---калибровка-rtc)
  - [ ] [sync_source](PROTOCOL.md#sync_source---переключить-источник-синхронизации)
  - [x] [sync_ntp](PROTOCOL.md#sync_ntp---синхронизация-rtc-по-ntp)
  - [x] [save_config](PROTOCOL.md#save_config---сохранить-конфигурацию)
  - [x] [load_config](PROTOCOL.md#load_config---загрузить-конфигурацию)
  - [x] [factory_reset](PROTOCOL.md#factory_reset---сброс-к-заводским-настройкам)
- [x] WiFi STA (клиент): включение/выключение, подключение к SSID
- [x] Парсер команд (JSON)

---

## Фаза 6: Интерфейс

- [x] UI (время, статус)
- [x] Мониторинг синхронизации

---

## Дополнительные задачи

- [x] При изменении некоторых значений рекомендовать перезагрузку для вступления изменений в силу
- [x] При загрузке использовать значения из конфига
- [x] Вынести NTP сервера из дефайнов в настройки
- [x] Добавить в ответ `sync_ntp` список используемых NTP серверов
- [ ] Обновление значка заряда батареи (данные с INA219)
- [x] Обновление значка GPS
- [ ] Настройка UI стартового экрана
- [ ] Настройка UI финишного экрана
- [x] Отправка метки "старт" в BLE и Serial
- [x] Отправка метки "финиш" в BLE и Serial
- [ ] Обратный отсчёт для стартующего на экране стартового модуля
- [x] Отправка метки "voice" для стартового модуля
- [x] Отправка метки "beep" для стартового модуля

---

## Протокол: status — доступность полей

**Доступно в коде без новых хелперов:**
- `device.name`, `device.number`, `device.type` (settings; нужен маппинг type->start/finish)
- `firmware.version` (VERSION)
- `wifi.state`, `wifi.rssi`, `wifi.ip` (wifiManager)
- `ble.state` (bleSerial)
- `rtc.ready`, `rtc.lost_power`, `rtc.temperature_c` (rtc)
- `gps.state`, `gps.fix`, `gps.satellites` (gps)
- `gps.pps_signal` (pps_is_locked)
- `sync.last_ms`, `sync.state`, `sync.accuracy_us`, `sync.source` (time_sync)
- `storage.used_pct`, `storage.ok` (settings.getStorageStats)

**Нужны новые хелперы/источники:**
- `firmware.build_date`
- `system.uptime_s`, `system.free_heap_bytes`, `system.reset_reason`
- `wifi.ssid` (текущая сеть, не из настроек)
- `ble.clients`
- `rtc.drifting`
- `gps.fix_age_ms`
- `power.battery_voltage` (INA219)
