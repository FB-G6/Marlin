# Implementation Plan: Диагностика и безопасная остановка сессии печати

**Branch**: `001-print-session-diagnostics` | **Date**: 2026-11-02 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-print-session-diagnostics/spec.md`

## Summary

Нужно сделать так, чтобы любое прекращение печати (штатное завершение, отмена
пользователем, внешняя команда, ошибка носителя, аварийная остановка, сброс)
было **наблюдаемым**: причина с кодом и источником показывается пользователю
модальным окном на TFT, сохраняется энергонезависимо (кольцевой журнал на
16 событий во внешней I²C-EEPROM) и читается с экрана принтера после
выключения питания.

Технический подход:

- новый модуль `Marlin/src/feature/print_session.{h,cpp}`, полностью закрытый
  флагом `FB_G6_PRINT_JOURNAL` (принцип II конституции);
- журнал хранится **отдельной областью** EEPROM (`MARLIN_EEPROM_SIZE - 768`),
  со своим magic/version/CRC, поэтому `SettingsData`, `EEPROM_VERSION` и
  пользовательские настройки не меняются и `Initialize EEPROM` не требуется
  (принцип VI);
- «активная сессия» (файл, контрольная длительность, процент, этап) пишется
  в два чередующихся слота; на старте прошивки наличие валидной активной
  сессии означает прерванную печать, и причина «сброс» синтезируется из
  `hal.get_reset_source()` (watchdog / питание / программный-кнопочный);
- источник остановки помечается в узких точках (экран, меню, WiFi M26,
  M524, извлечение карты, ошибка чтения SD) и потребляется в единственном
  месте —
  `abortSDPrinting()`; штатное завершение фиксируется в `finishSDPrinting()`,
  аварийное — в `kill()` до `minkill()`;
- UI: модальный экран причины (закрывается касанием) + новый пункт
  «Журнал остановок» в меню настроек; строки локализуются штатно (EN/RU).

## Technical Context

**Language/Version**: C++ `gnu++14` (прошивка), `gnu++17` (симулятор)

**Primary Dependencies**: Marlin 2.1.2 (форк FB-G6), PlatformIO, STM32duino
`IWatchdog`, `Wire` (I²C EEPROM), `TFT_COLOR_UI` (MKS TS35), MKS WiFi module

**Storage**: внешняя I²C-EEPROM (`ATMEL_24C32`-класс, адрес `0x50`,
`MARLIN_EEPROM_SIZE = 0x1000`, `I2C_EEPROM` на PB6/PB7). Журнал — отдельная
область 768 байт (`0xD00..0xFFF`), настройки — `SettingsData` от `0x64`.
SD-карта для журнала не используется.

**Testing**: `pio run -e mks_robin_nano_v3` (сборка + контроль размера),
`buildroot/bin/format_code`, ручные сценарии на принтере (quickstart.md).
Юнит-тестов в проекте нет; `MARLIN_TEST_BUILD` не используется.

**Target Platform**: MKS Robin Nano V3, STM32F407VGT6, 168 MHz, 192 КБ RAM,
1 МБ flash

**Project Type**: встроенная прошивка (single project, монолитная сборка
PlatformIO)

**Performance Goals**:

- запись события останова ≤ 300 мс один раз на событие и только из
  нормального контекста (`task()`, `main()`, `kill()`), никогда из ISR;
- контрольная запись активной сессии ≤ 60 мс не чаще 1 раза в 300 с и только
  при достаточном заполнении планировщика;
- чтение журнала при загрузке ≤ 60 мс;
- прирост flash ≤ 2 % (SC-006), RAM ≤ 1 КБ.

**Constraints**:

- нет `new`/`delete`, нет блокирующих вызовов в ISR; `kill()`/`loud_kill()`
  работают в нормальном контексте: пишут RAM-флаги, `noinit`-запись и
  best-effort событие в EEPROM; строки — `F()`/`PROGMEM`;
- I²C-EEPROM пишется побайтово с задержкой ≈7 мс на изменённый байт
  (`HAL/shared/eeprom_if_i2c.cpp`), поэтому пишем только изменяемые байты;
- модальное окно в `TFT_COLOR_UI` — отдельный экран (нет композитинга/альфы,
  один canvas-буфер);
- записи в общих файлах Marlin обязаны быть минимальными и под флагом
  `FB_G6_PRINT_JOURNAL`.

**Scale/Scope**: 16 событий в журнале, 2 слота активной сессии,
13 кодов причин (11 строк локализации), 6 источников, 2 языка, 1 новый модуль,
1 новый экран меню, 1 модальный экран, ~12 точек правки в общих файлах
(по 1–6 строк).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Принцип | Требование | Как выполняется в плане | Итог |
| --- | --- | --- | --- |
| I. Безопасность (NON-NEGOTIABLE) | Защиты не ослабляются; авария → предсказуемая остановка | Журнал **только читает** состояние и пишет в EEPROM; `thermalManager`, watchdog, `SanityCheck`, software endstops не меняются. `kill()`/`loud_kill()` работают в нормальном контексте, поэтому после `thermalManager.disable_all_heaters()` фиксируются RAM-флаги, `noinit`-запись и выполняется best-effort запись события в EEPROM; `minkill()` не отменяется. Поведение остановок (нагрев off, `G28XY`, Z без изменений, `M84` по завершении) не меняется, а документируется | PASS |
| II. Совместимость с апстримом | Кастом под `FB_G6_*`, правки общих файлов минимальны | Всё новое — под `FB_G6_PRINT_JOURNAL` (новый флаг в `Configuration_adv.h`); при выключенном флаге модуль компилируется в пустоту, а вставки в общие файлы исчезают | PASS |
| III. Конфигурация как единый источник истины | Параметры только в конфиге, геометрия — из констант Marlin | Глубина журнала, размер области, интервал контрольной записи — `#define` в `Configuration_adv.h`; адреса вычисляются из `MARLIN_EEPROM_SIZE`; координаты не используются вообще | PASS |
| IV. Проверка сборкой и на устройстве (NON-NEGOTIABLE) | `pio run -e mks_robin_nano_v3`, оба термистора, BLTouch ±, форматирование | Задачи: сборка целевого env, сборка 104GT и 3950, сборка с `FB_G6_BLTOUCH` и без, `format_code`, ручные сценарии на принтере (quickstart) | PASS |
| V. Ресурсы MCU и реального времени | Без динамической памяти, короткий ISR-код, именованные размеры | Статические буферы: 12 Б заголовка + 448 Б слотов + 64 Б активной сессии = 524 Б RAM, плюс 16 Б `noinit`-записи аварии; всё остальное — `PROGMEM`/`F()`; записи в EEPROM выполняются из `task()`/`main()`/`kill()`; `kill()` пишет RAM-флаги и `noinit`-запись, затем best-effort — событие в EEPROM (риск просадки питания, а не ISR), watchdog не затрагивается | PASS |
| VI. Интерфейс и протоколы как контракты | Не ломать экраны/касания, не менять формат WiFi, EEPROM — с миграцией, локализация RU/EN | Новый экран использует штатный `goto_screen`/`push_current_screen`; протокол MKS WiFi не меняется (только чтение команды M26); раскладка `SettingsData` не меняется — миграция не нужна; строки добавляются в `language_en.h` и `language_ru.h` | PASS |

Нарушений нет → раздел `Complexity Tracking` пуст. Дополнительно ниже
перечислены все правки общих файлов с обоснованием.

### Правки общих файлов Marlin (обоснование, принцип II)

| Файл | Правка | Зачем |
| --- | --- | --- |
| `Marlin/src/MarlinCore.cpp` | 8 вставок: захват источника сброса до `clear_reset_source()`, `init()`, старт/резюме сессии, запись в `abortSDPrinting()`, `finishSDPrinting()`, `kill()`, `stop()`, `idle()` | Единственные места, где известны факт и причина останова |
| `Marlin/src/sd/cardreader.cpp` | метка источника «носитель» в `release()` | Извлечение карты — отдельная причина (FR-004) |
| `Marlin/src/gcode/queue.cpp` | счётчик подряд идущих ошибок чтения SD → `abortFilePrintSoon()` | US1 сценарий 5: «карта становится недоступна» сейчас только логируется и не останавливает печать |
| `Marlin/src/gcode/sd/M524.cpp` | метка источника «консоль/хост» | FR-004: M524 отличим от экрана |
| `Marlin/src/module/wifi/wifi.cpp` | метка «сеть» в M26, метка «авария/M112» | FR-004, US3 |
| `Marlin/src/module/temperature.cpp` | метка «авария/термозащита» в `loud_kill()`, метка хоста в emergency-M524 | FR-005, FR-007 |
| `Marlin/src/lcd/tft/touch.cpp` | метка «локальный экран» в `STOP_PRINT` | FR-004 |
| `Marlin/src/lcd/menu/menu_main.cpp` | метка «локальный экран» в подтверждении остановки | FR-004 |
| `Marlin/Configuration_adv.h` | блок `@section FB-G6` с флагом и параметрами журнала | FR-001…FR-016 |
| `Marlin/src/lcd/language/language_en.h`, `language_ru.h` | строки `MSG_STOP_*` | FR-011 |
| `Marlin/src/lcd/menu/menu.h`, `menu_configuration.cpp` | объявление `menu_journal()` и `SUBMENU(MSG_STOP_JOURNAL, …)` | FR-014, FR-016, SC-007 |
| `Marlin/src/lcd/marlinui.h`/`.cpp` | объявление и вызов модального экрана причины | FR-002 |
| `Marlin/src/lcd/tft/ui_320x480.cpp` | отрисовка модального экрана причины (только 320×480) | FR-002 |
| `Marlin/src/module/settings.cpp` | `static_assert(EEPROM_OFFSET + sizeof(SettingsData) <= PRINT_JOURNAL_ADDR, ...)` | Защита от порчи настроек (VI) |
| `Marlin/src/inc/SanityCheck.h` | требование `EEPROM_SETTINGS` и проверка `EEPROM_OFFSET + sizeof(SettingsData) <= PRINT_JOURNAL_ADDR` | Явная ошибка конфигурации вместо тихой деградации |
| `Marlin/Version.h` | версия сборки | Принцип «документация и версия в том же изменении» |

## Project Structure

### Documentation (this feature)

```text
specs/001-print-session-diagnostics/
├── plan.md              # этот файл
├── research.md          # Phase 0: решения R1–R14
├── data-model.md        # Phase 1: сущности, перечисления, состояния
├── quickstart.md        # Phase 1: проверка на принтере и сборкой
├── contracts/
│   ├── journal-storage.md   # байтовая раскладка области EEPROM
│   ├── stop-reasons.md      # таблица кодов/источников/поведения
│   └── ui-contract.md       # модальное окно и экран журнала
├── checklists/
└── tasks.md             # Phase 2 (/speckit.tasks)
```

### Source Code (repository root)

```text
Marlin/
├── Configuration_adv.h                 # @section FB-G6: флаг и параметры
├── Version.h                           # FBG6-<версия>
└── src/
    ├── inc/SanityCheck.h               # проверка EEPROM_SETTINGS
    ├── MarlinCore.cpp                  # init, start, abort, finish, kill
    ├── feature/
    │   ├── print_session.h             # NEW: StopCause/StopSource/StopEvent, API
    │   └── print_session.cpp           # NEW: журнал, активная сессия, ring, EEPROM I/O
    ├── sd/cardreader.cpp               # метка «носитель»
    ├── gcode/queue.cpp                 # счётчик ошибок чтения SD
    ├── gcode/sd/M524.cpp               # метка «хост/консоль»
    ├── module/
    │   ├── wifi/wifi.cpp               # метки «сеть» и «авария»
    │   ├── temperature.cpp             # метки «термозащита»/«хост»
    │   └── settings.cpp                # static_assert раскладки
    └── lcd/
        ├── marlinui.h / marlinui.cpp   # показ модального экрана, screenFunc
        ├── tft/
        │   ├── touch.cpp               # метка «локальный экран»
        │   └── ui_320x480.cpp          # отрисовка модального экрана причины
        ├── menu/
        │   ├── menu.h                  # объявление menu_journal()
        │   ├── menu_journal.cpp        # NEW: список событий
        │   └── menu_configuration.cpp  # SUBMENU(MSG_STOP_JOURNAL, menu_journal)
        └── language/
            ├── language_en.h           # новые MSG_*
            └── language_ru.h           # переводы
```

**Structure Decision**: single project. Новый модуль — `src/feature/` (фича
компилируется всегда, содержимое под `#if ENABLED(FB_G6_PRINT_JOURNAL)`);
`default_src_filter` в `platformio.ini` содержит `+<src/*>`, поэтому добавления
файла в конфигурацию сборки не требуется. Новый экран меню — `src/lcd/menu/`
(каталог включается через `HAS_MARLINUI_MENU` в `ini/features.ini`); факт
попадания в сборку проверяется командой из раздела 4 quickstart.

## Реализация по фазам (для `/speckit.tasks`)

### Фаза A. Модель и хранилище (FR-001, 003, 008, 010, 012)

1. `Configuration_adv.h`: блок `@section FB-G6` с
   `FB_G6_PRINT_JOURNAL`, `FB_G6_PRINT_JOURNAL_SLOTS 16`,
   `FB_G6_PRINT_JOURNAL_REGION 768`,
   `FB_G6_PRINT_JOURNAL_CHECKPOINT_S 300`, `FB_G6_PRINT_JOURNAL_MODAL`,
   `FB_G6_SD_READ_ERROR_LIMIT 3`.
2. `src/inc/SanityCheck.h`: `#if ENABLED(FB_G6_PRINT_JOURNAL) && DISABLED(EEPROM_SETTINGS)` → `#error`; проверка `EEPROM_OFFSET + sizeof(SettingsData) <= PRINT_JOURNAL_ADDR`.
3. `print_session.h`: перечисления `StopCause`, `StopSource`, `SessionStage`,
   структуры `StopEvent`, `ActiveSession`, `EmergencyRecord`, константы раскладки журнала, API:
   `set_boot_reset(mcu)`, `init()`, `task()`, `note_source(src)`,
   `note_emergency(cause)`, `take_source()`, `cause_for_source(src)`,
   `emergency_cause()`, `start_session()`, `checkpoint(bool force)`,
   `end_session(cause, source)`, `last_event()`, `event_at(i)`, `count()`.
4. `print_session.cpp`: загрузка/сохранение области, ring-буфер, CRC8 на запись,
   CRC16 на заголовок, самовосстановление заголовка сканированием слотов,
   ротация двух слотов активной сессии.
5. `settings.cpp`: `static_assert(EEPROM_OFFSET + sizeof(SettingsData) <= PRINT_JOURNAL_ADDR, ...)`.

### Фаза B. Точки фиксации (FR-001, 004, 005, 007)

6. `MarlinCore.cpp`: `print_session.set_boot_reset(mcu)` до `clear_reset_source()`;
   `print_session.init()` после `settings.first_load()`.
7. `startOrResumeJob()` → `print_session.start_session()` (M24, экран, WiFi,
   автозапуск — единая воронка).
8. `abortSDPrinting()` → `print_session.end_session(print_session.cause_for_source(src), src)`
   для `src = print_session.take_source()` **до** `EVENT_GCODE_SD_ABORT` (иначе
   `G28XY` начнёт движение раньше записи); несопоставленная метка → `CAUSE_UNKNOWN`.
9. `finishSDPrinting()` → `print_session.end_session(CAUSE_COMPLETED,
   SOURCE_FIRMWARE)`.
10. `kill()` → `print_session.note_emergency()`: `emergency_cause`/
    `emergency_source`/`FLAG_EMERGENCY` в RAM, `noinit`-запись
    `EmergencyRecord` (magic+CRC), затем best-effort `append(StopEvent)` в
    EEPROM — после отключения нагревателей и до `minkill()`. `kill()`/`loud_kill()`
    работают в нормальном контексте; ограничение записи связано с риском
    просадки питания, а не с ISR (R7). Если запись не удалась, `init()` при
    следующей загрузке синтезирует событие из `noinit`-записи.
11. Метки источника: `touch.cpp` (STOP_PRINT), `menu_main.cpp` (подтверждение),
    `wifi.cpp` M26 → `SOURCE_NETWORK`, `M524.cpp` и emergency-M524 →
    `SOURCE_HOST`, `cardreader.cpp::release()` → `SOURCE_MEDIA`,
    `touch.cpp`/`menu_main.cpp` → `SOURCE_LOCAL_UI`.
12. `gcode/queue.cpp::get_sdcard_commands()`: счётчик подряд идущих ошибок
    чтения (`n < 0 && !card_eof`); при достижении
    `FB_G6_SD_READ_ERROR_LIMIT` — метка `SOURCE_MEDIA` + `abortFilePrintSoon()`;
    успешное чтение сбрасывает счётчик. Без счётчика «карта стала недоступна»
    (US1 сценарий 5) не приводит к остановке и записи причины.
13. `task()` в `idle()`: контрольная запись активной сессии (только при
    `planner.moves_free() == 0` или на паузе), показ модального окна,
    обработка отложенной записи.

### Фаза C. Отображение (FR-002, 011, 014, 016)

14. `MarlinUI::stop_reason_screen()` в `ui_320x480.cpp`: непрозрачная карточка,
    причина + источник + длительность + файл + процент; полноэкранное
    касание → `ui.goto_previous_screen()`; вызов через
    `ui.push_current_screen(); ui.goto_screen(MarlinUI::stop_reason_screen)`.
    Не показывается при `MF_KILLED` (аварийный экран имеет приоритет), при
    `wait_for_user`, для последнего события `COMPLETED`/`EMERGENCY_*`/
    `RESET_*` и при выключенном `FB_G6_PRINT_JOURNAL_MODAL`.
15. `menu_journal.cpp`: `START_MENU()`/`BACK_ITEM(MSG_MAIN)`, по две строки на
    событие (`#NN причина источник MM:SS`, `файл P%`), пустой журнал →
    `STATIC_ITEM(MSG_STOP_JOURNAL_EMPTY)`.
16. `menu_configuration.cpp`: `SUBMENU(MSG_STOP_JOURNAL, menu_journal)` сразу
    после `BACK_ITEM` (до `ADVANCED_SETTINGS`), чтобы пункт был виден без
    прокрутки на 320×480; доступ с главного экрана за 2 касания (SC-007).
17. Локализация: `MSG_STOP_JOURNAL`, `MSG_STOP_JOURNAL_EMPTY`,
    `MSG_STOP_CAUSE_*` (11 ключей на 13 кодов: `0x10`–`0x12` используют
    `MSG_STOP_CAUSE_ABORTED`), `MSG_STOP_SOURCE_*` (6),
    `MSG_STOP_REASON_TITLE`, `MSG_STOP_TAP_TO_CLOSE` — EN + RU; перечень и
    тексты — `contracts/ui-contract.md` §3.

### Фаза D. Проверки (принцип IV)

18. `pio run -e mks_robin_nano_v3` — сборка, отсутствие новых warning'ов,
    сравнение `RAM`/`Flash` с базовой сборкой (≤ 2 %).
19. Сборка вариантов `params_104gt.h` и `params_3950.h`; сборка с
    `params_bltouch104gt.h`/`params_bltouch3950.h`.
20. `buildroot/bin/format_code` для изменённых файлов.
21. Ручные сценарии из `quickstart.md` на принтере; результат — в PR.

## Риски и их снижение

| # | Риск | Снижение |
| --- | --- | --- |
| R1 | Долгая побайтовая запись EEPROM (≈7 мс/байт) тормозит печать | Пишем только изменённые байты (сравнение уже есть в `eeprom_wired.cpp`); событие — 1 слот (≤ 300 мс) в момент останова; контрольная запись ≤ 60 мс и только при `planner.moves_free() == 0` |
| R2 | Сбой питания посреди записи | per-entry CRC8 + заголовочный CRC16; повреждённая запись пропускается, заголовок восстанавливается сканированием; активная сессия — два чередующихся слота |
| R3 | Запись причины аварии при `kill()`/`loud_kill()` | `kill()` работает в нормальном контексте: пишет RAM-флаг, `noinit`-запись и best-effort `StopEvent` в EEPROM. Если запись не удалась, при загрузке событие синтезируется из `noinit`; при снятии питания `noinit` теряется, синтезируется `CAUSE_RESET_POWER` с `FLAG_EMERGENCY_DETAIL_LOST` («авария, деталь не зафиксирована»). Нужно доказать сборкой/на железе, что `.noinit` не обнуляется загрузчиком; иначе fallback — только вторая ветка |
| R4 | Модальное окно конфликтует с `status_screen()` (перехват клика) и с экраном kill | Отдельный экран через `goto_screen`, не оверлей; при `MF_KILLED`/`wait_for_user` показ откладывается |
| R5 | `EEPROM_INIT_NOW` при новой сборке перезаписывает настройки | Тег раскладки берётся из `FB_G6_EEPROM_LAYOUT_ID` (`Configuration_adv.h`), а не из `__DATE__ __TIME__`, поэтому пересборка настройки не стирает; тег меняется только вместе с `SettingsData`. Журнал лежит вне `SettingsData` и не участвует в `save()`/`load()`, поэтому переживает и смену тега |
| R6 | Область журнала пересекается с `SettingsData` (UBL выключен) | `static_assert` в `settings.cpp` + проверка `MARLIN_EEPROM_SIZE` в модуле |
| R7 | Расхождение «модальное окно поверх экрана» и реализации (отдельный экран) | Зафиксировано в `contracts/ui-contract.md`: у TFT нет композитинга, модальность реализуется через стек экранов, возврат — на статус-экран (фактическое поведение `ui.abort_print()`) |

## Complexity Tracking

> Нарушений конституции нет; таблица не заполняется.

## Повторная проверка конституции после Phase 1

Дизайн (research.md R1–R14, data-model.md, contracts/) не добавил нарушений:

| Принцип | Что уточнилось после проектирования | Итог |
| --- | --- | --- |
| I | Журнал не влияет на защиты; порядок «нагреватели off → запись → `minkill()`» зафиксирован в `contracts/stop-reasons.md` | PASS |
| II | Все вставки в общие файлы — однострочные и под `FB_G6_PRINT_JOURNAL`; новый модуль и меню — отдельные файлы | PASS |
| III | Параметры (слоты, область, интервал, лимит ошибок SD) — только в `Configuration_adv.h` | PASS |
| IV | `quickstart.md` содержит 4 сборки, форматирование и 14 сценариев на принтере | PASS |
| V | RAM — 524 Б статически + 16 Б `noinit`-записи; `kill()` работает в нормальном контексте и делает best-effort запись без задержек; размеры — именованные константы; прирост flash ≤ 2 % проверяется в T048 | PASS |
| VI | `SettingsData` и `EEPROM_VERSION` не меняются → миграция не требуется; формат WiFi не меняется; строки EN/RU добавлены; модальность реализуется стеком экранов (уточнение зафиксировано в `contracts/ui-contract.md`) | PASS |
