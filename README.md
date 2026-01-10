# Entime Firmware

Прошивка для ESP32-S3 с TFT дисплеем, GPS + RTC и управлением через JSON-команды по Serial/BLE.

## Документация

- [PROTOCOL.md](PROTOCOL.md) — протокол команд и ответы
- [SETTINGS.md](SETTINGS.md) — структура и ограничения настроек
- [BOM.md](BOM.md) — список компонентов
- [TODO.md](TODO.md) — ближайшие задачи

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

## Настройка IDE

### IntelliSense / clangd

Проект использует `.clangd` для настройки IntelliSense. После клонирования:

1. Сгенерируйте `compile_commands.json`:
   ```bash
   pio run -t compiledb
   ```

2. Создайте глобальный конфиг clangd с путём к toolchain:

   **Linux** (`~/.config/clangd/config.yaml`):
   ```yaml
   CompileFlags:
     Add:
       - -I/home/<username>/.platformio/packages/toolchain-xtensa-esp32s3/xtensa-esp32s3-elf/include
   ```

   **Windows** (`%USERPROFILE%\AppData\Local\clangd\config.yaml`):
   ```yaml
   CompileFlags:
     Add:
       - -IC:/Users/<username>/.platformio/packages/toolchain-xtensa-esp32s3/xtensa-esp32s3-elf/include
   ```

3. Перезапустите IDE (Ctrl+Shift+P → "Developer: Reload Window")
