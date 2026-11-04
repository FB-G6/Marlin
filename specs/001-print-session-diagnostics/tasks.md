---

description: "Task list for feature 001-print-session-diagnostics"
---

# Tasks: Диагностика и безопасная остановка сессии печати

**Input**: Design documents from `/specs/001-print-session-diagnostics/`

**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/journal-storage.md`, `contracts/stop-reasons.md`,
`contracts/ui-contract.md`, `quickstart.md`

**Tests**: Юнит-тестов в проекте нет (`MARLIN_TEST_BUILD` не используется).
«Тестами» считаются сборки целевого env и ручные сценарии из `quickstart.md`
(принцип IV конституции). Отдельные тест-задачи не создаются.

**Organization**: задачи сгруппированы по пользовательским историям, чтобы
каждая история давала независимо проверяемый инкремент.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: можно выполнять параллельно (другой файл, нет зависимостей)
- **[Story]**: `US1`, `US2`, `US3` — только для фаз пользовательских историй
- В описании всегда указан точный путь к файлу

## Path Conventions

Единый проект (прошивка Marlin). Пути — от корня репозитория:
`Marlin/Configuration_adv.h`, `Marlin/src/...`, `specs/001-print-session-diagnostics/...`.

## Принципы конституции, затрагиваемые задачами

| Принцип | Где в задачах |
| --- | --- |
| I. Безопасность (NON-NEGOTIABLE) | T018 (порядок «нагреватели off → RAM-флаг/`noinit` → `minkill()`), T037, T038, T053 |
| II. Совместимость с апстримом | T003, T004, T012–T026, T039–T041 (все вставки под `FB_G6_PRINT_JOURNAL`) |
| III. Конфигурация как источник истины | T003, T006 (адреса из `MARLIN_EEPROM_SIZE`) |
| IV. Проверка сборкой и на устройстве (NON-NEGOTIABLE) | T001, T011, T033, T039, T045–T052 |
| V. Ресурсы MCU и реального времени | T005–T009 (статические буферы), T018 (best-effort запись в нормальном контексте), T019 (запись вне движения) |
| VI. Интерфейс и протоколы как контракты | T010, T027–T033 (EEPROM/UI/локализация) |

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: зафиксировать точку отсчёта и убедиться, что новые файлы попадут
в сборку без правки конфигурации сборки.

- [X] T001 Зафиксировать базовый размер прошивки для SC-006: выполнить `pio run -e mks_robin_nano_v3 -t clean` и `pio run -e mks_robin_nano_v3 | tee /tmp/fb-g6-base-build.log`, сохранить строки `RAM`/`Flash` из `/tmp/fb-g6-base-build.log`
- [X] T002 [P] Проверить, что новые файлы попадут в сборку без правки конфигурации: `platformio.ini` (`default_src_filter` содержит `+<src/*>`) и `ini/features.ini` (`HAS_MARLINUI_MENU = src_filter=+<src/lcd/menu>`); зафиксировать результат в описании PR

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: флаг фичи, модель данных и слой хранения журнала. Без этой фазы
ни одна история не может быть реализована.

**⚠️ CRITICAL**: задачи T003–T010 и T054 должны быть завершены до начала US1.

- [X] T003 Добавить в `Marlin/Configuration_adv.h` блок `@section FB-G6: print session journal` с `FB_G6_PRINT_JOURNAL`, `FB_G6_PRINT_JOURNAL_SLOTS 16` (1..32), `FB_G6_PRINT_JOURNAL_REGION 768`, `FB_G6_PRINT_JOURNAL_CHECKPOINT_S 300` (0 = только старт/стоп), `FB_G6_PRINT_JOURNAL_MODAL`, `FB_G6_SD_READ_ERROR_LIMIT 3` — в стиле Marlin (`@section`, комментарий, допустимые значения)
- [X] T004 Добавить в `Marlin/src/inc/SanityCheck.h` проверки: `#if ENABLED(FB_G6_PRINT_JOURNAL) && DISABLED(EEPROM_SETTINGS)` → `#error`, и `#error`, если `MARLIN_EEPROM_SIZE < FB_G6_PRINT_JOURNAL_REGION + EEPROM_OFFSET + sizeof(SettingsData)` (по контракту `contracts/journal-storage.md` §7)
- [X] T005 Создать `Marlin/src/feature/print_session.h`: перечисления `StopCause` (`0x00`, `0x10`–`0x12`, `0x20`, `0x30`–`0x32`, `0x40`, `0x41`, `0x48`, `0x50`, `0x7F`), `StopSource` (`0x00`–`0x05`), `SessionStage` (`0x00`–`0x04`), биты `StopFlags`; структуры `StopEvent` (28 Б), `JournalHeader` (12 Б), `ActiveSession` (32 Б), `EmergencyRecord` (16 Б: magic `0x31455246`, cause, source, crc8, резерв) с `#pragma pack(push,1)`; объявления API `set_boot_reset(mcu)`, `init()`, `task()`, `note_source(src)`, `note_emergency(cause)`, `take_source()`, `cause_for_source(src)`, `emergency_cause()`, `start_session()`, `checkpoint(bool force)`, `end_session(cause, source)`, `last_event()`, `event_at(i)`, `count()`, а также константы раскладки, доступные из других единиц трансляции: `PRINT_JOURNAL_ADDR = MARLIN_EEPROM_SIZE - FB_G6_PRINT_JOURNAL_REGION`, `PRINT_JOURNAL_HEADER_ADDR`, `PRINT_JOURNAL_SLOTS_ADDR`, `PRINT_JOURNAL_ACTIVE_ADDR` (нужны для `SanityCheck.h` в T004 и `static_assert` в `settings.cpp` в T010) — всё под `#if ENABLED(FB_G6_PRINT_JOURNAL)`
- [X] T006 Реализовать в `Marlin/src/feature/print_session.cpp` статические буферы (12 + 448 + 64 Б, без `new`/`delete`), CRC8 (poly `0x07`, init `0x00`) и CRC16 заголовка через `Marlin/src/libs/crc16.h`; константы раскладки объявлены в заголовке (T005)
- [X] T007 Реализовать в `Marlin/src/feature/print_session.cpp` `init()`: чтение области через `persistentStore.access_start()/read_data()/access_finish()`, валидация V1–V5 из `data-model.md` §4, восстановление заголовка сканированием слотов (V3), принудительная терминация `file[12] = '\0'` (V6); при несовпадении `magic`/`version`/`slots` журнал считается пустым без ошибки и без `settings.reset()`; затем `init_from_emergency_record()` — разбор `noinit`-записи аварии (валидная запись → `StopEvent(CAUSE_EMERGENCY_*, source, FLAG_ESTIMATED_DURATION)`, после чего запись очищается; невалидная → пометка `FLAG_EMERGENCY_DETAIL_LOST` для синтезированного события сброса)
- [X] T008 Реализовать в `Marlin/src/feature/print_session.cpp` `append()`: запись слота `PRINT_JOURNAL_ADDR + 12 + head*28` (28 Б), затем заголовка (12 Б) — строго в этом порядке; `head = (head+1) % slots`, `count = min(count+1, slots)`; при `write_data() == true` (ошибка) событие считается не записанным; при ошибке записи заголовка слот помечается недействительным (`crc8 = 0`), чтобы V3-сканирование его не воскресило
- [X] T009 Реализовать в `Marlin/src/feature/print_session.cpp` работу с активной сессией: два чередующихся слота по 32 Б (`slot = seq & 1`), выбор валидного слота с большим `seq`, `checkpoint(bool force)` с ограничением частоты `FB_G6_PRINT_JOURNAL_CHECKPOINT_S`, закрытие слота (`active = 0`, `seq++`)
- [X] T054 [P] Реализовать в `Marlin/src/feature/print_session.cpp` `end_session(cause, source)`: идемпотентность (нет активной сессии → события нет), порядок «сначала `append(StopEvent)`, затем закрытие слота активной сессии», выбор причины по `cause_for_source()`/`take_source()` (`data-model.md` §5, FR-001, FR-007)
- [X] T010 Добавить в `Marlin/src/module/settings.cpp` под `#if ENABLED(FB_G6_PRINT_JOURNAL)` `static_assert(EEPROM_OFFSET + sizeof(SettingsData) <= PRINT_JOURNAL_ADDR, "FB_G6 print journal overlaps EEPROM settings.")` рядом с существующими `static_assert` (строки 668–702)
- [X] T011 **Checkpoint**: `pio run -e mks_robin_nano_v3` собирается без новых warning'ов; при выключенном `FB_G6_PRINT_JOURNAL` модуль компилируется в пустоту (проверить сборкой с закомментированным флагом и вернуть флаг)

**Checkpoint**: модель и хранилище готовы — истории можно реализовывать.

---

## Phase 3: User Story 1 - Видимая причина остановки (Priority: P1) 🎯 MVP

**Goal**: любое прекращение печати (завершение, отмена, ошибка носителя, сброс)
фиксируется с кодом причины и источником, показывается модальным окном и
сохраняется в EEPROM-журнале, доступном после выключения питания.

**Independent Test**: вызвать остановку тремя способами (WiFi M26, экран/консоль,
извлечение SD-карты) и убедиться, что каждый раз на экране и в журнале указаны
разные корректные причины и источники; затем выключить/включить питание и
убедиться, что записи читаются.

### Точки фиксации останова

- [X] T012 [P] [US1] В `Marlin/src/MarlinCore.cpp` (строка 1155) сохранить `const byte mcu = hal.get_reset_source()` в модуль (`print_session.set_boot_reset(mcu)`) **до** `hal.clear_reset_source()` (строка 1156)
- [X] T013 [US1] В `Marlin/src/MarlinCore.cpp` вызвать `print_session.init()` сразу после `SETUP_RUN(settings.first_load())` (строка 1339), чтобы журнал читался после загрузки настроек
- [X] T014 [US1] Реализовать в `Marlin/src/feature/print_session.cpp` синтез события сброса **в конце `init()`** (после `init_from_emergency_record()` и до первого `task()`): если валидный слот активной сессии имеет `active == 1`, создать `StopEvent` с `CAUSE_RESET_WATCHDOG`/`CAUSE_RESET_POWER`/`CAUSE_RESET_SOFTWARE` (маппинг `RST_WATCHDOG` → watchdog, `RST_POWER_ON` → питание, `RST_SOFTWARE`/`RST_EXTERNAL` → программный/кнопочный; отдельного флага brownout в STM32 HAL нет), `FLAG_ESTIMATED_DURATION`, `SOURCE_FIRMWARE`, затем закрыть слот; при `active == 0` событие не создаётся. Если последнее событие журнала уже терминальное (`COMPLETED`, `ABORT_*`, `MEDIA_ERROR`, `INTERNAL_STOP`, `EMERGENCY_*`) и его `duration_s` не меньше `ActiveSession.elapsed_s`, синтез не выполняется (защита от дубля). Если валидна `noinit`-запись аварии (T007) и события ещё нет в журнале, вместо `CAUSE_RESET_*` фиксируется `CAUSE_EMERGENCY_*` с источником из записи; при утерянной детали — `CAUSE_RESET_POWER` + `FLAG_EMERGENCY_DETAIL_LOST`
- [X] T015 [US1] В `Marlin/src/MarlinCore.cpp::startOrResumeJob()` (строка 347) вызвать `print_session.start_session()` — единая воронка для M24, экрана, WiFi и автозапуска. Если активная сессия уже существует (резюме после паузы), новую не начинать: только `checkpoint(true)` и обновление источника; длительность не сбрасывать
- [X] T016 [US1] В `Marlin/src/MarlinCore.cpp::abortSDPrinting()` (строка 361) вызвать `print_session.end_session(print_session.cause_for_source(src), src)` для `src = print_session.take_source()` **до** `queue.inject(F(EVENT_GCODE_SD_ABORT))`, чтобы `G28XY` не начал движение раньше записи; несопоставленная метка источника → `CAUSE_UNKNOWN`
- [X] T017 [US1] В `Marlin/src/MarlinCore.cpp::finishSDPrinting()` (строка 385) вызвать `print_session.end_session(CAUSE_COMPLETED, SOURCE_FIRMWARE)` после успешной постановки `M1001` в очередь
- [X] T018 [US1] В `Marlin/src/MarlinCore.cpp::kill()` (строка 898) вызвать `print_session.note_emergency(print_session.emergency_cause())` **после** `thermalManager.disable_all_heaters()` и **до** `minkill(steppers_off)` (строка 921). `kill()`/`loud_kill()` работают в нормальном контексте: пишутся RAM-флаги и `noinit`-запись `EmergencyRecord` (magic+CRC), затем best-effort `append(StopEvent)` в EEPROM (без `safe_delay`); уже установленный подтип аварии не перезаписывать (first-wins)
- [X] T019 [US1] Реализовать в `Marlin/src/feature/print_session.cpp` `task()`: контрольная запись активной сессии (только при `planner.moves_free() == 0` или на паузе, не чаще `FB_G6_PRINT_JOURNAL_CHECKPOINT_S`), отложенный показ модального окна, обработка отложенной записи
- [X] T020 [US1] В `Marlin/src/MarlinCore.cpp::idle()` (строка 780) вызвать `print_session.task()` после `thermalManager.task()`

### Метки источника останова

- [X] T021 [P] [US1] В `Marlin/src/lcd/tft/touch.cpp` (case `STOP_PRINT`, строка 428) вызвать `print_session.note_source(SOURCE_LOCAL_UI)` перед `ui.abort_print()`
- [X] T022 [P] [US1] В `Marlin/src/lcd/menu/menu_main.cpp` (подтверждение остановки, строка 309, `ui.abort_print`) вызвать `print_session.note_source(SOURCE_LOCAL_UI)` перед `ui.abort_print`
- [X] T023 [P] [US1] В `Marlin/src/module/wifi/wifi.cpp` (case 26 / M26, строка 1058) вызвать `print_session.note_source(SOURCE_NETWORK)` перед `ui.abort_print()`
- [X] T024 [P] [US1] В `Marlin/src/gcode/sd/M524.cpp` (строка 46, `abortFilePrintSoon()`) вызвать `print_session.note_source(SOURCE_HOST)` перед `card.abortFilePrintSoon()`
- [X] T025 [P] [US1] В `Marlin/src/sd/cardreader.cpp::release()` (строка 538) вызвать `print_session.note_source(SOURCE_MEDIA)` перед `abortFilePrintSoon()`
- [X] T026 [US1] В `Marlin/src/gcode/queue.cpp::get_sdcard_commands()` (строка 565) добавить `uint8_t`-счётчик подряд идущих ошибок чтения (`n < 0 && !card_eof`); при достижении `FB_G6_SD_READ_ERROR_LIMIT` — `print_session.note_source(SOURCE_MEDIA)` + `card.abortFilePrintSoon()`; успешное чтение сбрасывает счётчик

### Отображение: модальное окно и журнал

- [X] T027 [P] [US1] Добавить в `Marlin/src/lcd/language/language_en.h` строки `MSG_STOP_JOURNAL`, `MSG_STOP_JOURNAL_EMPTY`, `MSG_STOP_REASON_TITLE`, `MSG_STOP_TAP_TO_CLOSE`, `MSG_STOP_CAUSE_*` (11 ключей на 13 кодов), `MSG_STOP_SOURCE_*` (6) — тексты из `contracts/ui-contract.md` §3; все строки под `#if ENABLED(FB_G6_PRINT_JOURNAL)` … `#endif`
- [X] T028 [P] [US1] Добавить в `Marlin/src/lcd/language/language_ru.h` русские переводы всех ключей из T027 (таблица `contracts/ui-contract.md` §3); все строки под `#if ENABLED(FB_G6_PRINT_JOURNAL)` … `#endif`
- [X] T029 [US1] Объявить в `Marlin/src/lcd/marlinui.h` (рядом с `kill_screen`, строка 519) `static void stop_reason_screen();` и `static void show_stop_reason();`; реализовать `show_stop_reason()` в `Marlin/src/lcd/marlinui.cpp` через `push_current_screen()` + `goto_screen(stop_reason_screen)` с защитой от повторного `push`, если текущий экран уже `stop_reason_screen`
- [X] T030 [US1] Реализовать `MarlinUI::stop_reason_screen()` в `Marlin/src/lcd/tft/ui_320x480.cpp` (по образцу `draw_kill_screen()`, строка 150): непрозрачная карточка, причина + источник + длительность (`~` при `FLAG_ESTIMATED_DURATION`) + файл 8.3 + процент, полноэкранный контрол `BACK` (`touch.add_control(BACK, 0, 0, TFT_WIDTH, TFT_HEIGHT)`) → `ui.goto_previous_screen()`; не показывать при `MF_KILLED`, `wait_for_user`, `EMERGENCY_*` и при выключенном `FB_G6_PRINT_JOURNAL_MODAL`; для последнего события `COMPLETED`/`RESET_*` окно не показывать (уже есть экран завершения / причина доступна в журнале)
- [X] T031 [US1] Объявить `void menu_journal();` в `Marlin/src/lcd/menu/menu.h` (рядом с `menu_main()`, строка 206)
- [X] T032 [US1] Создать `Marlin/src/lcd/menu/menu_journal.cpp`: `START_MENU()` + `BACK_ITEM(MSG_MAIN)`, по два `STATIC_ITEM_F` на событие (`#NN причина · источник · ЧЧ:ММ:СС` и `файл · P%`), новые сверху, пустой журнал → `STATIC_ITEM(MSG_STOP_JOURNAL_EMPTY)`; повреждённые записи не отображаются
- [X] T033 [US1] Добавить в `Marlin/src/lcd/menu/menu_configuration.cpp` (функция `menu_configuration()`, строка 471) пункт `SUBMENU(MSG_STOP_JOURNAL, menu_journal)` сразу после `BACK_ITEM` (до `ADVANCED_SETTINGS`), чтобы он был виден без прокрутки на 320×480; доступ с главного экрана за 2 касания (SC-007)
- [ ] T034 [US1] **Checkpoint**: `pio run -e mks_robin_nano_v3` собирается без новых warning'ов; ручная проверка S1, S2, S4, S5, S6, S7, S9, S11, S12, S13, S14 из `quickstart.md`
- [ ] T052 [US1] **Checkpoint**: длительный прогон — 3 печати ≥ 4 ч с периодическими остановками (SC-002) и серия ≥ 10 остановок без «немых» событий (SC-004) по сценарию S15 из `quickstart.md`; результат и номер сборки — в PR

**Checkpoint**: US1 полностью функциональна и проверяема независимо — MVP готов.

---

## Phase 4: User Story 2 - Предсказуемый и задокументированный путь остановки (Priority: P1)

**Goal**: поведение при каждом виде останова (нагреватели, приводы, парковка,
стол) задокументировано и совпадает с фактическим; нет «немых» состояний и
остаточного состояния предыдущей сессии.

**Independent Test**: для каждого вида останова (завершение, отмена, внешняя
команда, авария, сброс) сверить фактическое поведение с таблицей
`contracts/stop-reasons.md` §3; расхождений быть не должно.

**Зависимость**: требует завершённой US1 (модуль и точки фиксации).

- [X] T035 [US2] Реализовать в `Marlin/src/feature/print_session.cpp` сброс остаточного состояния в `start_session()`: обнулить `pending_source`, счётчик ошибок чтения SD, флаг отложенного показа окна и `FLAG_UNRECORDED_EMERGENCY`, чтобы предыдущая сессия не влияла на новую печать (US2 сценарий 3)
- [X] T036 [US2] Реализовать в `Marlin/src/feature/print_session.cpp` идемпотентность `end_session()`: при отсутствии активной сессии событие не создаётся (защита от задвоения при двойном вызове abort/finish и от ложного события «сброс» после штатного завершения)
- [X] T037 [US2] Зафиксировать в `Marlin/src/feature/print_session.h` doc-комментарий со ссылкой на `specs/001-print-session-diagnostics/contracts/stop-reasons.md` §3 как на эталон поведения при остановке (FR-006) и явно указать, что фича поведение не меняет
- [X] T038 [US2] Проверить аддитивность правок пути останова: в `Marlin/src/MarlinCore.cpp` диффы `abortSDPrinting()`, `finishSDPrinting()`, `kill()` содержат только добавленные строки под `FB_G6_PRINT_JOURNAL`; `thermalManager.disable_all_heaters()`, `quickstop_stepper()`, `EVENT_GCODE_SD_ABORT "G28XY"`, `SD_FINISHED_RELEASECOMMAND "M84"` и `minkill()` не изменены — зафиксировать в описании PR
- [X] T053 [US2] В `Marlin/src/MarlinCore.cpp::stop()` (строка 970) под `FB_G6_PRINT_JOURNAL` вызвать `print_session.end_session(CAUSE_INTERNAL_STOP, SOURCE_FIRMWARE)` после `thermalManager.disable_all_heaters()` и до `marlin_state = MF_STOPPED` — код `0x50` из `contracts/stop-reasons.md` §1 иначе никогда не фиксируется (FR-001)
- [ ] T039 [US2] **Checkpoint**: ручная проверка S1, S7, S8, S10 из `quickstart.md` — поведение осей/нагрева/стола совпадает с таблицей `contracts/stop-reasons.md` §3, задвоения событий нет

**Checkpoint**: US1 и US2 работают независимо; поведение остановки документировано и подтверждено.

---

## Phase 5: User Story 3 - Различение внешних и внутренних остановок (Priority: P2)

**Goal**: каждое событие несёт стабильный код причины и источник; внешние
каналы (сеть, консоль) отличимы от внутренних причин; аварии различаются по
подтипу; неизвестная причина фиксируется, а не теряется.

**Independent Test**: вызвать остановку из каждого внешнего канала (WiFi M26,
M524 из консоли, M112 из консоли и из WiFi) и убедиться, что код причины и
источник совпадают с таблицей `contracts/stop-reasons.md` §1–2 и однозначно
отличимы от внутренних причин.

**Зависимость**: требует завершённой US1 (метки источника и модуль). От US2 не
зависит.

- [X] T040 [P] [US3] В `Marlin/src/module/temperature.cpp::loud_kill()` (строка 1243) вызвать `print_session.note_emergency(CAUSE_EMERGENCY_THERMAL)` перед `kill()`
- [X] T041 [P] [US3] В `Marlin/src/module/temperature.cpp` (обработчик M112, строка 1863) вызвать `print_session.note_emergency(CAUSE_EMERGENCY_M112)` и `print_session.note_source(SOURCE_HOST)` перед `kill(...)`
- [X] T042 [P] [US3] В `Marlin/src/module/wifi/wifi.cpp` (обработчик M112, строка 2080) вызвать `print_session.note_emergency(CAUSE_EMERGENCY_M112)` и `print_session.note_source(SOURCE_NETWORK)` перед `kill(...)`
- [X] T043 [US3] Реализовать в `Marlin/src/feature/print_session.cpp` `emergency_cause()`: вернуть выставленный подтип, иначе `CAUSE_EMERGENCY_OTHER`. Для `abortSDPrinting()` без метки источника использовать отдельную ветку `cause_for_abort()` → `CAUSE_UNKNOWN` + `SOURCE_UNKNOWN` (FR-007), запись сохраняется с длительностью, процентом, этапом и именем файла
- [X] T044 [US3] Реализовать в `Marlin/src/feature/print_session.cpp` хранение «последней известной активности» (этап, процент, длительность) для события `CAUSE_UNKNOWN` и для синтезированного события сброса (US3 сценарий 3)
- [ ] T045 [US3] **Checkpoint**: ручная проверка S2, S3, S10 из `quickstart.md` — источники «Сеть»/«Консоль»/«Прошивка» и подтипы аварий различаются; таблица кодов `contracts/stop-reasons.md` §1–2 совпадает с фактическим поведением

**Checkpoint**: все три истории функциональны и проверяемы независимо.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: обязательные проверки принципа IV, версия и материалы PR.

- [ ] T046 Отформатировать изменённые файлы: `buildroot/bin/format_code feature/print_session.cpp feature/print_session.h` и остальные файлы из списка `plan.md`; проверить `git diff --stat`, что посторонние файлы не затронуты
- [X] T047 Собрать все четыре варианта: `params_104gt.h`, `params_3950.h`, `params_bltouch104gt.h`, `params_bltouch3950.h` (копирование в `Marlin/build_params.h` + `pio run -e mks_robin_nano_v3`), затем `git checkout -- Marlin/build_params.h`
- [X] T048 Сравнить `RAM`/`Flash` с базовым логом из T001: прирост flash ≤ 2 % (SC-006), RAM ≤ 1 КБ; результат приложить к PR
- [X] T049 Обновить `Marlin/Version.h`: `SHORT_BUILD_VERSION` для обеих веток `TEMP_SENSOR_0` (5 → 104GT, 13 → 3950) согласованно
- [ ] T050 Пройти все сценарии S1–S16 из `specs/001-print-session-diagnostics/quickstart.md` на принтере; зафиксировать фактическое поведение, отметить непроверенное (например, S10/S16 при отказе от проверки)
- [ ] T051 Подготовить материалы PR по `quickstart.md` §6: логи сборок до/после, результат четырёх сборок, список пройденных сценариев, использованный `Marlin/build_params.h`, вывод о необходимости `Initialize EEPROM` (ожидается: не требуется)


---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: без зависимостей, начинается сразу
- **Foundational (Phase 2)**: зависит от Setup; **блокирует все истории**
- **US1 (Phase 3)**: зависит от Foundational
- **US2 (Phase 4)**: зависит от US1 (использует модуль и точки фиксации)
- **US3 (Phase 5)**: зависит от US1 (использует метки источника и модуль)
- **Polish (Phase 6)**: зависит от всех выбранных историй

### User Story Dependencies

- **US1 (P1)**: после Foundational — базовая история, даёт MVP
- **US2 (P1)**: после US1 — не зависит от US3
- **US3 (P2)**: после US1 — не зависит от US2

US2 и US3 можно вести параллельно после завершения US1.

### Within Each User Story

- Модель/хранилище (Foundational) → точки фиксации → отображение → проверка
- В US1: сначала `MarlinCore.cpp` (T012–T020), затем метки источника
  (T021–T026), затем локализация (T027–T028), затем UI (T029–T033)
- В US3: подтипы аварий (T040–T042) → логика `emergency_cause()`/`CAUSE_UNKNOWN`
  (T043–T044) → проверка (T045)

### Parallel Opportunities

- T002 параллельно с T001
- T021–T025 — пять разных файлов, полностью параллельны
- T027 и T028 — разные файлы локализации, параллельны
- T040, T041, T042 — разные файлы/обработчики, параллельны
- T046–T049 — форматирование, сборки, размер и версия можно вести параллельно
  после завершения кода

---

## Parallel Example: User Story 1

```bash
# Метки источника — пять независимых файлов:
Task: "touch.cpp: note_source(SOURCE_LOCAL_UI) перед ui.abort_print()"
Task: "menu_main.cpp: note_source(SOURCE_LOCAL_UI) перед ui.abort_print"
Task: "wifi.cpp: note_source(SOURCE_NETWORK) в case 26 (M26)"
Task: "M524.cpp: note_source(SOURCE_HOST) перед abortFilePrintSoon()"
Task: "cardreader.cpp::release(): note_source(SOURCE_MEDIA)"

# Локализация — два независимых файла:
Task: "language_en.h: MSG_STOP_* (EN)"
Task: "language_ru.h: MSG_STOP_* (RU)"
```

## Parallel Example: User Story 3

```bash
Task: "temperature.cpp::loud_kill(): note_emergency(CAUSE_EMERGENCY_THERMAL)"
Task: "temperature.cpp: M112 → note_emergency(CAUSE_EMERGENCY_M112) + SOURCE_HOST"
Task: "wifi.cpp: M112 → note_emergency(CAUSE_EMERGENCY_M112) + SOURCE_NETWORK"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1: Setup (T001–T002)
2. Phase 2: Foundational (T003–T011, T054) — **блокирует всё**
3. Phase 3: US1 (T012–T034, T052)
4. **STOP and VALIDATE**: S1, S2, S4, S5, S6, S7, S9, S11, S12, S13, S14, S15 из `quickstart.md`
5. Прошивка готова к демонстрации: причина останова видна и сохраняется

### Incremental Delivery

1. Setup + Foundational → модель и хранилище готовы
2. US1 → проверка → MVP (видимая и сохранённая причина)
3. US2 → проверка → документированное поведение остановки подтверждено
4. US3 → проверка → внешние и внутренние остановки различаются
5. Polish → сборки, форматирование, версия, PR

### Parallel Team Strategy

1. Команда завершает Setup + Foundational вместе
2. После US1:
   - Разработчик A: US2 (идемпотентность, остаточное состояние, проверка)
   - Разработчик B: US3 (подтипы аварий, `CAUSE_UNKNOWN`, проверка каналов)
3. Polish выполняется после слияния обеих историй

---

## Notes

- `[P]` — разные файлы, нет зависимостей от незавершённых задач
- `[Story]` связывает задачу с историей из `spec.md` для трассируемости
- Каждая вставка в общий файл Marlin обязана быть под `FB_G6_PRINT_JOURNAL`
  (принцип II); при выключенном флаге код исчезает
- Запись в EEPROM — только изменяемых байт; событие пишется один раз в момент
  останова, контрольная запись — не чаще `FB_G6_PRINT_JOURNAL_CHECKPOINT_S`
- Не отключать thermal protection, watchdog, `SanityCheck` и software endstops
  (принцип I); единственная новая проверка — `EEPROM_SETTINGS` в T004
- `kill()`/`loud_kill()` работают в нормальном контексте; ограничение записи
  EEPROM связано с риском просадки питания, а не с ISR: сначала RAM и
  `noinit`-запись `EmergencyRecord`, затем best-effort запись события в журнал;
  если запись не удалась, `init()` синтезирует событие при следующей загрузке
- Нумерация T052–T054 продолжена, чтобы не ломать существующие ссылки
  T001–T051: T054 выполняется в Phase 2 (между T009 и T011), T052 — в Phase 3
  после T034, T053 — в Phase 4 после T038
- Коммит после каждой задачи или логической группы; форматирование и поведение
  не смешивать в одном коммите
- **Статус T046 (2026-11-04)**: остаётся открытой. В окружении доступен только
  `uncrustify 0.83`, а `buildroot/share/extras/uncrustify.cfg` написан под
  версию 0.74: прогон переформатирует файл целиком (сдвигает `#if`-блоки и
  `enum`), создавая посторонний дифф. Новый код (T035, T044, T053) оформлен
  вручную по стилю репозитория; форматирование нужно повторить на машине с
  `uncrustify 0.74`.
- **Статус проверок на железе (2026-11-04)**: T034, T039, T045, T050, T052,
  T051 требуют принтера и не выполнены в этой среде. Сборки (T034 в части
  компиляции, T047, T048) пройдены: `mks_robin_nano_v3` собирается без
  warning'ов; RAM 72 948 Б (+632 Б к базе), flash 361 564 Б (+5 816 Б, +1,64 %).
  Четыре варианта (`params_104gt.h`, `params_3950.h`,
  `params_bltouch104gt.h`, `params_bltouch3950.h`) собираются успешно.
