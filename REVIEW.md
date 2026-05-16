# Ревью `src/timing/time_sync.cpp`

Ревью текущей реализации после отката экспериментальных изменений. Рабочее
дерево на момент ревью было чистым.

## Findings

1. **High:** [`time_sync.cpp:213`](src/timing/time_sync.cpp#L213) ломает режим
   `auto_sync=false`. После первого GPS sync `s_status.synced=true`, но
   `settimeofday()` не вызывается. Следующий PPS сравнивает GPS-кандидат с
   системным временем через `gettimeofday()`, которое осталось старым, и
   alignment начинает отваливаться по `kGpsCandidateJumpGuardUs`. Guard должен
   сравнивать с текущим UTC-якорем, а не всегда с системными часами.

2. **High:** [`time_sync.cpp:177`](src/timing/time_sync.cpp#L177) медианный
   фильтр применяется к `delta`, знак которого выбирает UTC-секунду PPS. Это
   опасно: фильтр может взять значение от предыдущей NMEA-секунды и дать ошибку
   ровно на +/-1 секунду. Фильтровать можно диагностику/качество, но не знак,
   которым выбирается секунда.

3. **High:** [`time_sync.cpp:544`](src/timing/time_sync.cpp#L544),
   [`time_sync.cpp:580`](src/timing/time_sync.cpp#L580) и
   [`time_sync.cpp:669`](src/timing/time_sync.cpp#L669) выставляют `source=RTC`
   и `synced=true` даже когда валидного RTC-якоря может не быть. В результате
   `time_sync_state()` может показать RTC-состояние, а
   `time_sync_esp_to_utc_us()` вернет `false`. Это несогласованный контракт
   статуса.

4. **Medium:** [`time_sync.cpp:626`](src/timing/time_sync.cpp#L626) и
   [`time_sync.cpp:769`](src/timing/time_sync.cpp#L769) large-step guards могут
   застрять навсегда. Если delta остается больше 100 ms после warmup, коррекция
   каждый раз пропускается, guard не сбрасывается, но состояние продолжает
   выглядеть синхронизированным/почти нормальным.

5. **Medium:** [`rtc_sqw_isr.cpp:38`](src/timing/rtc_sqw_isr.cpp#L38) читает
   `volatile int64_t s_last_edge_us` без критической секции. PPS ISR аналогичный
   timestamp читает через `noInterrupts()`, а SQW нет. На ESP32 это риск
   разорванного 64-битного чтения.

6. **Medium:** [`gps.cpp:48`](src/hal/gps/gps.cpp#L48) и
   [`time_sync.cpp:390`](src/timing/time_sync.cpp#L390) не гарантируют, что
   timestamp относится именно к NMEA-предложению, которое обновило UTC.
   `lastSentenceStartUs()` - это старт последнего полного предложения, а
   `MicroNMEA` хранит агрегированное состояние. Для PPS/NMEA phase alignment это
   слабый контракт.

7. **Low:** [`time_sync.cpp:16`](src/timing/time_sync.cpp#L16) `s_rtc_synced`
   фактически не используется для поведения, только сбрасывается/устанавливается.
   Это шум в состоянии.

## Решение по улучшению

Не переписывать все сразу. Минимальный правильный первый проход:

- исправить PPS/NMEA alignment: убрать медиану из выбора секунды, guard
  сравнивать с UTC-якорем, а не с `gettimeofday()` в `auto_sync=false`;
- привести `synced/source/anchor` к строгому инварианту: `synced=true` только
  если `time_sync_esp_to_utc_us()` может работать;
- починить атомарное чтение SQW timestamp;
- не добавлять GPS holdover и крупный рефакторинг в этот же патч.

После этого отдельно резать монолит на функции, но без изменения поведения.
