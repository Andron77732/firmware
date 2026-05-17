# NMEA timestamping notes

Дата: 2026-05-17

Этот документ фиксирует известное ограничение текущей модели timestamping GPS NMEA.

## Что измеряется сейчас

GPS PPS timestamp берется в ISR по `esp_timer_get_time()`, то есть близко к
реальному фронту PPS.

NMEA timestamp сейчас берется в `GPS::update()` при обработке символа `$`:

```cpp
if (c == '$') {
  _current_sentence_start_us = esp_timer_get_time();
}
```

Это timestamp не момента прихода байта в UART, а момента, когда main loop
прочитал этот байт из UART buffer.

## Почему это важно

`align_pps_utc()` выбирает UTC секунду для PPS по фазе между PPS timestamp и
timestamp NMEA sentence start:

```text
delta = pps_esp_us - nmea_esp_us

delta >= 0  => NMEA пришло до PPS, PPS = nmea_utc_sec + 1
delta <  0  => NMEA пришло после PPS, PPS = nmea_utc_sec
```

Если main loop был занят и поздно прочитал UART buffer, `nmea_esp_us` сдвигается
вперед относительно реального приема. При достаточно большой задержке это может:

- сорвать phase alignment и перевести состояние в `GPS_DEGRADED`;
- в худшем случае выбрать соседнюю UTC секунду, если защита не поймала границу;
- ухудшить диагностику `last_phase_delta_us`.

## Почему это пока не срочный дефект

В текущей time sync логике уже есть несколько защит:

- freshness guard для NMEA/UTC update;
- deadband около опасной зоны +/-1 секунды;
- jump guard относительно текущего UTC anchor;
- коррекция ошибки на +/-1 секунду от свежего GPS anchor;
- fallback в `GPS_DEGRADED`, если fresh NMEA alignment не проходит.

PPS остается точным секундным фронтом. Слабое место именно в выборе номера UTC
секунды по timestamp NMEA, если main loop долго не читал UART.

## Когда возвращаться к исправлению

К этой теме стоит вернуться, если в логах появятся:

- частые переходы `GPS_OK -> GPS_DEGRADED` при стабильном PPS и нормальном NMEA;
- большие или скачущие `last_phase_delta_us`;
- ошибки ровно на +/-1 секунду в timestamp событий;
- заметные задержки main loop из-за BLE/UI/I2C, совпадающие с NMEA parsing.

## Возможные улучшения

Предпочтительные варианты, если ограничение станет практической проблемой:

- читать GPS UART в отдельной более приоритетной task;
- использовать ESP-IDF UART driver/event queue и timestamp ближе к RX event;
- timestamp'ить не `$` в main loop, а событие приема данных на уровне драйвера;
- построить модель задержки конкретного NMEA sentence относительно PPS и
  использовать ее вместо processing-time timestamp;
- добавить тесты с искусственными задержками loop и NMEA около PPS границы.

Пока это остается documented known limitation.
