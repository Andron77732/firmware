# Entime Firmware

Прошивка для ESP32-S3 с TFT дисплеем.

## Требования

- [PlatformIO](https://platformio.org/)
- ESP32-S3 DevKitC

## Сборка

```bash
pio run
```

## Прошивка

```bash
pio run -t upload
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
