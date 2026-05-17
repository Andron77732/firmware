# Touch UI

Текущее поведение touch-интерфейса прошивки.

## Обработка событий

Touch-события маршрутизируются в таком порядке:

1. `StatusBar`
2. `MainArea`
3. `Footer`

Действия выполняются на событии `Release`. На `Press` зона только
захватывается и логируется как `Touch route: ui=...`.

## Реализованные действия

### WiFi icon

Зона: `StatusBarWifi`.

Тап по WiFi-иконке включает или выключает WiFi:

- если WiFi в состоянии `UNINITIALIZED` или `OFF`, прошивка берёт сохранённые
  `wifi.ssid` и `wifi.passwd` и запускает подключение;
- если `wifi.ssid` пустой, WiFi не включается, на экран добавляется
  `ERROR: WiFi is not configured`;
- если запуск подключения не удался, на экран добавляется
  `ERROR: WiFi connect failed`;
- если WiFi уже включён, подключается, переподключается или находится в ошибке,
  тап выключает WiFi через `wifiManager.end()`;
- если остановка WiFi не завершилась успешно, на экран добавляется
  `ERROR: WiFi stop timeout`.

### Status bar background

Зона: `StatusBarBackground`.

Тап по свободной области status bar переключает `MainArea`:

- если сейчас показан `LOADING`, возвращает рабочий экран устройства;
- иначе переключает `MainArea` в `LOADING`.

Рабочий экран выбирается по типу устройства:

- `device.type == 1` - `START`;
- `device.type == 2` - `FINISH`.

### Loading log drag

Зона: `MainArea`, только когда `MainArea` находится в режиме `LOADING`.

Drag вверх/вниз прокручивает лог загрузки:

- drag вниз показывает более старые строки;
- drag вверх возвращает к новым строкам;
- если лог находится внизу, новые строки автоматически остаются видимыми;
- если пользователь прокрутил лог вверх, новые строки не сбрасывают позицию
  просмотра.

## Распознаваемые зоны без действия

Эти зоны уже определяются и логируются, но специальных действий пока не имеют:

- `StatusBarClock`
- `StatusBarGps`
- `StatusBarBluetooth`
- `StatusBarBattery`
- `Footer`

## Связанные файлы

- `src/app/touch_action_handler.cpp` - действия по touch target.
- `src/ui/status_bar.cpp` - определение зон status bar.
- `src/ui/main_area.cpp` - определение зоны main area.
- `src/ui/footer.cpp` - определение зоны footer.
- `src/ui/ui_touch_target.h` - список touch target.
