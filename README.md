# Entime Firmware

Прошивка для ESP32-S3 с TFT дисплеем, GPS + RTC + INA219 и управлением через JSON-команды по Serial/BLE.

## Документация

- [PROTOCOL.md](PROTOCOL.md) — протокол команд и ответы
- [SETTINGS.md](SETTINGS.md) — структура и ограничения настроек
- [BOM.md](BOM.md) — список компонентов
- [TODO.md](TODO.md) — ближайшие задачи

## Лицензия

MIT, см. [LICENSE](LICENSE).

## Требования

- [PlatformIO](https://platformio.org/)
- ESP32-S3 DevKitC (см. [BOM.md](BOM.md) для полной конфигурации)

## Быстрый старт

Сборка:
```bash
pio run
```

Прошивка:
```bash
pio run -t upload
```

Монитор порта:
```bash
pio device monitor
```
