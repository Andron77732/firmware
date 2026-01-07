# TODO - ENTime Firmware

Список задач для разработки проекта ENTime. Основной план проекта: [.cursor/prompts/plan-entime.prompt.md](.cursor/prompts/plan-entime.prompt.md)

---

## Фаза 3: Синхронизация

- [x] Синхронизация RTC по NTP

---

## Фаза 5: Коммуникация

- [ ] Serial (UART0) команды
  - [ ] [time](PROTOCOL.md#time---получить-текущее-время)
  - [ ] [status](PROTOCOL.md#status---получить-статус-синхронизации)
  - [ ] [gps](PROTOCOL.md#gps---управление-gps-модулем)
  - [ ] [calibrate](PROTOCOL.md#calibrate---калибровка-rtc)
  - [ ] [sync_source](PROTOCOL.md#sync_source---переключить-источник-синхронизации)
  - [ ] [sync_ntp](PROTOCOL.md#sync_ntp---синхронизация-rtc-по-ntp)
  - [ ] [save_config](PROTOCOL.md#save_config---сохранить-конфигурацию)
  - [ ] [load_config](PROTOCOL.md#load_config---загрузить-конфигурацию)
  - [ ] [factory_reset](PROTOCOL.md#factory_reset---сброс-к-заводским-настройкам)
- [x] WiFi STA (клиент): включение/выключение, подключение к SSID
- [x] Парсер команд (JSON)

---

## Фаза 6: Интерфейс

- [x] UI (время, статус)
- [ ] Мониторинг синхронизации

---

## Дополнительные задачи

- [ ] Настройка UI стартового экрана
- [ ] Настройка UI финишного экрана
- [ ] Отправка метки "старт" в BLE и Serial
- [ ] Отправка метки "финиш" в BLE и Serial
- [ ] Обратный отсчёт для стартующего на экране стартового модуля
- [ ] Отправка метки "voice" для стартового модуля

