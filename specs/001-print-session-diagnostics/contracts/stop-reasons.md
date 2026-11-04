# Contract: коды причин, источники и поведение остановки

**Feature**: `001-print-session-diagnostics` | **Версия контракта**: 1

Контракт фиксирует (а) стабильные коды причин и источников, (б) что именно
происходит с нагревателями, приводами, парковкой и столом при каждой причине
(FR-006), (в) какие строки показываются пользователю (FR-011).

## 1. Таблица причин

| Код | `StopCause` | Триггер в коде | Источник (`StopSource`) | Локализация (EN / RU) |
| --- | --- | --- | --- | --- |
| `0x00` | `CAUSE_COMPLETED` | `finishSDPrinting()` (`MarlinCore.cpp:385`) | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_COMPLETED` — "Completed" / "Завершено" |
| `0x10` | `CAUSE_ABORT_LOCAL_UI` | `touch.cpp` `STOP_PRINT`, `menu_main.cpp` подтверждение | `SOURCE_LOCAL_UI` | `MSG_STOP_CAUSE_ABORTED` — "Cancelled" / "Отменено" |
| `0x11` | `CAUSE_ABORT_NETWORK` | `wifi.cpp` `case 26` (M26) | `SOURCE_NETWORK` | `MSG_STOP_CAUSE_ABORTED` + источник «Сеть» |
| `0x12` | `CAUSE_ABORT_HOST` | `M524.cpp`, emergency-парсер M524 | `SOURCE_HOST` | `MSG_STOP_CAUSE_ABORTED` + источник «Консоль» |
| `0x20` | `CAUSE_MEDIA_ERROR` | `cardreader.cpp::release()`, счётчик ошибок чтения в `queue.cpp` | `SOURCE_MEDIA` | `MSG_STOP_CAUSE_MEDIA` — "Media error" / "Ошибка носителя" |
| `0x30` | `CAUSE_RESET_WATCHDOG` | загрузка + `RST_WATCHDOG` | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_RESET_WATCHDOG` — "Reset: watchdog" / "Сброс: сторожевой таймер" |
| `0x31` | `CAUSE_RESET_POWER` | загрузка + `RST_POWER_ON` (brownout HAL не выставляет отдельный флаг) | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_RESET_POWER` — "Reset: power" / "Сброс: питание" |
| `0x32` | `CAUSE_RESET_SOFTWARE` | загрузка + `RST_SOFTWARE`/`RST_EXTERNAL` | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_RESET_SOFTWARE` — "Reset: software/button" / "Сброс: программа/кнопка" |
| `0x40` | `CAUSE_EMERGENCY_THERMAL` | `loud_kill()` (`temperature.cpp:1243`) | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_EMERGENCY_THERMAL` — "Emergency: thermal" / "Авария: термозащита" |
| `0x41` | `CAUSE_EMERGENCY_M112` | M112 (emergency-парсер, WiFi) | `SOURCE_HOST`/`SOURCE_NETWORK` | `MSG_STOP_CAUSE_EMERGENCY_M112` — "Emergency: M112" / "Авария: M112" |
| `0x48` | `CAUSE_EMERGENCY_OTHER` | прочие вызовы `kill()` | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_EMERGENCY` — "Emergency stop" / "Аварийная остановка" |
| `0x50` | `CAUSE_INTERNAL_STOP` | `stop()` (`MarlinCore.cpp:970`) | `SOURCE_FIRMWARE` | `MSG_STOP_CAUSE_INTERNAL` — "Stopped by firmware" / "Остановлено прошивкой" |
| `0x7F` | `CAUSE_UNKNOWN` | `abortSDPrinting()` без метки источника | `SOURCE_UNKNOWN` | `MSG_STOP_CAUSE_UNKNOWN` — "Unknown cause" / "Причина неизвестна" |

Правила:
- коды неизменны; новые значения добавляются только внутри своей группы
  (`0x1x`, `0x2x`, `0x3x`, `0x4x`, `0x5x`);
- неизвестный код при чтении → `MSG_STOP_CAUSE_UNKNOWN`, запись сохраняется;
- `CAUSE_UNKNOWN` — не ошибка, а обязательная фиксация «немой» остановки
  (FR-007): в записи остаются длительность, процент, этап и имя файла.

## 2. Источники

| Код | `StopSource` | Локализация (EN / RU) |
| --- | --- | --- |
| `0x00` | `SOURCE_UNKNOWN` | `MSG_STOP_SOURCE_UNKNOWN` — "Unknown" / "Неизвестно" |
| `0x01` | `SOURCE_LOCAL_UI` | `MSG_STOP_SOURCE_LOCAL` — "Screen" / "Экран" |
| `0x02` | `SOURCE_NETWORK` | `MSG_STOP_SOURCE_NETWORK` — "Network" / "Сеть" |
| `0x03` | `SOURCE_HOST` | `MSG_STOP_SOURCE_HOST` — "Console" / "Консоль" |
| `0x04` | `SOURCE_MEDIA` | `MSG_STOP_SOURCE_MEDIA` — "SD card" / "SD-карта" |
| `0x05` | `SOURCE_FIRMWARE` | `MSG_STOP_SOURCE_FIRMWARE` — "Firmware" / "Прошивка" |

## 3. Поведение при остановке (FR-006, FR-015)

Поведение **не меняется** этой фичей; таблица фиксирует фактическое состояние
кода на момент реализации и служит эталоном для проверки на принтере.

| Причина | Нагреватели | Приводы | Парковка / стол | Экран | Возобновление |
| --- | --- | --- | --- | --- | --- |
| `COMPLETED` | выключаются только финальным G-кодом файла (`M104/M140 S0`); прошивка их не гасит | `SD_FINISHED_RELEASECOMMAND "M84"` → все приводы обесточены | парковки нет; стол остаётся на месте | экран «Printing Finished» (`ui.screen_num = 3`) | новая печать |
| `ABORT_LOCAL_UI` / `ABORT_NETWORK` / `ABORT_HOST` / `MEDIA_ERROR` | `thermalManager.disable_all_heaters()` (`SD_ABORT_NO_COOLDOWN` выключен) | `quickstop_stepper()`, очередь очищена; приводы остаются включены | `EVENT_GCODE_SD_ABORT "G28XY"` → парковка X/Y в домашнюю позицию; **Z не меняется** | модальное окно причины | новая печать |
| `EMERGENCY_*` | выключаются в `kill()` и повторно в `minkill()` | `stepper.disable_e_steppers()` (M112 — `disable_all_steppers()`), PSU off при `PSU_CONTROL` | парковки нет; машина обездвижена | аварийный экран (`kill_screen`), модальное окно причины **не показывается** | только сброс/перезагрузка (причина пишется в журнал best-effort сразу в `kill()`, иначе — из `noinit` при следующей загрузке) |
| `INTERNAL_STOP` | `thermalManager.disable_all_heaters()` | движение остановлено, приводы не обесточены | парковки нет | сообщение `MSG_STOPPED` | `M999` |
| `RESET_*` | обесточены вместе с платой | обесточены; после загрузки `axis_known_position = false` | движения при загрузке нет | модальное окно не показывается; причина видна в журнале | новая печать (после `G28`) |

Примечания:
- `G28XY` выполняется **после** записи события в журнал (порядок в
  `abortSDPrinting()`), иначе движение началось бы до фиксации причины;
- подъём Z при отмене не выполняется и не является частью фичи (FR-015);
- при `MEDIA_ERROR` карта уже может быть недоступна — журнал пишется в EEPROM,
  а не на носитель, поэтому причина сохраняется (US1 сценарий 5);
- `kill()`/`loud_kill()` работают в нормальном контексте (не в ISR). Запись
  EEPROM ограничена риском просадки питания: причина сначала фиксируется в
  `noinit`-записи, затем выполняется best-effort запись события в журнал; если
  она не удалась, `init()` синтезирует событие при следующей загрузке. После
  снятия питания без `noinit` остаётся `RESET_POWER` с
  `FLAG_EMERGENCY_DETAIL_LOST`.
- Если выставлен `FLAG_EMERGENCY_DETAIL_LOST`, UI показывает «Сброс: питание
  (детали аварии потеряны)»; код `0x31` при этом не меняется.

## 4. Обнаружение ошибки носителя

| Событие | Текущее поведение | Поведение после фичи |
| --- | --- | --- |
| Извлечение карты (`HAS_SD_DETECT`) | `CardReader::release()` → `abortFilePrintSoon()` | то же + метка `SOURCE_MEDIA` → `CAUSE_MEDIA_ERROR` |
| Ошибка чтения байта (`card.get() < 0`, не EOF) | `SERIAL_ERROR_MSG(STR_SD_ERR_READ)` и продолжение | счётчик подряд идущих ошибок; при `FB_G6_SD_READ_ERROR_LIMIT` (по умолчанию 3) → метка `SOURCE_MEDIA` + `abortFilePrintSoon()`; успешное чтение сбрасывает счётчик |
| Карта не смонтирована при старте печати | печать не начинается | событие не создаётся (сессии ещё нет) |

Счётчик — `uint8_t` в модуле, без влияния на темп печати (проверка в
`get_sdcard_commands()` уже выполняется на каждый байт).

## 5. Что видит пользователь

- Модальное окно (см. `ui-contract.md`): заголовок `MSG_STOP_REASON_TITLE`,
  строка причины, строка источника, длительность `ЧЧ:ММ:СС` (с `~`, если
  длительность оценочная), имя файла 8.3, процент.
- Журнал: те же поля в компактном виде, новые события сверху.
- Никаких новых команд чтения журнала по консоли или сети (FR-016).
