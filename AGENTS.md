# AGENTS.md — Flying Bear Ghost 6 (форк Marlin)

Правила для агентов и людей, работающих в этом репозитории. Читать перед любыми
изменениями кода.

## 1. Что это за проект

Кастомная прошивка Marlin 2.1.2 для 3D-принтера **Flying Bear Ghost 6**
(FB-G6). Это форк `FB-G6/Marlin`, текущий remote — `polarikus/Marlin`,
рабочая ветка — `2.1.2`.

Ключевые отличия от апстрима Marlin:

- плата **MKS Robin Nano V3** (STM32F407VGT6, ARM Cortex-M4);
- экран **MKS TS35 V2.0** (320×480, вертикально) и UI **TFT_COLOR_UI**;
- модуль **MKS WiFi** (`MKS_WIFI_MODULE`), печать по сети, MKS UI/upload;
- кастомные экраны и меню под Ghost 6 (tramming, finish screen, урезанные меню);
- сборки под разные термисторы (104GT/3950) и с/без BLTouch;
- русский и английский языки интерфейса.

Апстрим-документация Marlin применима, но **приоритет у кода этого репозитория**.

## 2. Стек и инструменты

| Что | Значение |
| --- | --- |
| Язык | C++ (`-std=gnu++14` для прошивки, `gnu++17` для симулятора), немного C и Python |
| Сборка | PlatformIO (`pio`), CMake не используется |
| Плата | `BOARD_MKS_ROBIN_NANO_V3` |
| Основной env | `mks_robin_nano_v3` |
| Компилятор | `arm-none-eabi-gcc` (ставится PlatformIO) |
| Форматирование | uncrustify + `buildroot/share/extras/uncrustify.cfg` |
| Тесты | bash-скрипты в `buildroot/tests`, `buildroot/bin/run_tests` |
| CI | GitHub Actions (`.github/workflows`) |

`platformio.ini` — точка входа, конфигурация разнесена по `ini/*.ini` и
`Marlin/config.ini`. Не дублировать настройки в `platformio.ini`, если для них
есть отдельный `ini/`-файл.

## 3. Сборка

### Основная прошивка

```bash
pio run -e mks_robin_nano_v3
```

Результат: `.pio/build/mks_robin_nano_v3/firmware.bin`.

Прошивка кладётся в корень microSD и устанавливается при включении принтера.
После установки EEPROM нужно инициализировать в меню.

### Варианты термистора и BLTouch

Вариант задаётся файлом `Marlin/build_params.h`, который подключается первым
в `Configuration.h`. Готовые варианты лежат в `Marlin/build_params.variants/`:

| Файл | Термистор | BLTouch |
| --- | --- | --- |
| `params_104gt.h` | 104GT-2 / 104NT | нет |
| `params_3950.h` | 3950 | нет |
| `params_bltouch104gt.h` | 104GT-2 / 104NT | да |
| `params_bltouch3950.h` | 3950 | да |

Порядок сборки варианта:

```bash
cp Marlin/build_params.variants/params_bltouch3950.h Marlin/build_params.h
pio run -e mks_robin_nano_v3
```

Не коммитить изменения `Marlin/build_params.h` как «фичу»: это переключатель
локальной сборки. CI сам подставляет нужный вариант при релизе.

### Симулятор

```bash
# в Configuration.h раскомментировать:
# #define MOTHERBOARD BOARD_LINUX_RAMPS

pio run -e simulator_linux_release      # Linux
pio run -e simulator_macos_release      # macOS
.pio/build/simulator_linux_release/MarlinSimulator
```

Для симулятора на macOS используйте `simulator_macos_release`, а не linux-env.
Нужны SDL2, SDL2_net, OpenGL и GLM (см. комментарий в `ini/native.ini`).

### Проверка конфигурации

Перед сборкой PlatformIO запускает скрипты из `buildroot/share/PlatformIO/scripts/`
(`preflight-checks.py`, `configuration.py` и др.), а Marlin — `SanityCheck.h`.
Ошибки конфигурации видны на этапе компиляции; не отключайте проверки, чтобы
«просто собралось».

## 4. Тесты и проверки

Юнит-тестов как таковых нет: тесты — это сборки эталонных конфигураций.
В `Marlin/src/tests` есть каркас `MARLIN_TEST_BUILD` (startup/periodic tests),
но по умолчанию он пуст и исключён из обычной сборки через
`default_src_filter`.

```bash
# одна конфигурация
make tests-single-local TEST_TARGET=mks_robin_nano_v1v2

# только тесты, содержащие подстроку/номер
make tests-single-local TEST_TARGET=mks_robin_nano_v1v2 ONLY_TEST=1

# все конфигурации из CI
make tests-all-local

# то же в docker
make tests-all-local-docker
```

Список целей берётся из `.github/workflows/test-builds.yml` через
`get_test_targets.py`. Отдельной цели `mks_robin_nano_v3` в `buildroot/tests`
нет — для этой платы основной проверкой остаётся `pio run -e mks_robin_nano_v3`.

`make`-цели `tests-single-ci` используют `GIT_RESET_HARD` и **сбрасывают
локальные изменения**. Не запускать их на рабочей копии с несохранёнными
правками.

### Форматирование

```bash
# пути указываются относительно Marlin/src
buildroot/bin/format_code module/planner.cpp
buildroot/bin/format_code module          # весь каталог
```

Требуется установленный `uncrustify`. Форматируйте только те файлы, которые
меняли: полный прогон по репозиторию создаёт огромный дифф.

### CI

- `.github/workflows/test-builds.yml` выполняется только в апстрим-репозитории
  (`if: github.repository == 'MarlinFirmware/Marlin'`), в этом форке он
  фактически не работает;
- `.github/workflows/release.yml` собирает 4 варианта прошивки по тегу и
  прикладывает zip к релизу.

Поэтому локальная сборка `mks_robin_nano_v3` — обязательный шаг перед PR.

## 5. Структура репозитория

```text
Marlin/
  Configuration.h          базовая конфигурация принтера
  Configuration_adv.h      расширенные возможности
  build_params.h           активный вариант сборки (термистор/BLTouch)
  build_params.variants/   готовые варианты build_params.h
  Version.h                версия прошивки (FBG6-…)
  src/
    MarlinCore.cpp/.h      main loop, инициализация
    module/                planner, stepper, temperature, motion, wifi
    gcode/                 обработчики G-кода (по категориям)
    feature/               подключаемые фичи (pause, runout, leds, …)
    lcd/                   UI: tft (TFT_COLOR_UI), menu, e3v2, extui
    HAL/                   аппаратная абстракция (STM32, AVR, native…)
    pins/                  карты пинов плат
    libs/                  встроенные библиотеки
    inc/                   Conditionals, SanityCheck, Version
ini/                       PlatformIO-окружения по платформам
buildroot/                 скрипты сборки, тесты, uncrustify, утилиты
docs/                      внутренняя документация Marlin
```

Кастомный код FB-G6 ищите по маркерам `FB_G6_*`, `FBG6`, `FINISH_SCREEN`,
`DISABLE_*_MENU`, `MKS_WIFI_MODULE`.

Служебные каталоги процесса разработки:

```text
.specify/                   Spec Kit: скрипты, шаблоны, конституция, workflow
.opencode/commands/         команды /speckit.* и /speckit.bugfix.*
.opencode/skills/           скиллы Clean Code
specs/                      спецификации фич (создаётся при первом /speckit.specify)
```

## 6. Как устроена кастомизация

1. **Конфигурация** — через `Configuration.h` / `Configuration_adv.h`.
   Новые опции оформляйте в стиле Marlin: блок `@section`, подробный
   комментарий, допустимые значения, затем `#define`.
2. **Фичи** — под `#if ENABLED(...)` / `#ifdef ...`, чтобы код собирался и без
   фичи. Не ломайте сборку апстрим-конфигураций.
3. **Кастомные экраны** — в `Marlin/src/lcd/tft/` (`ui_320x480.cpp`,
   `touch.cpp`, `touch.h`) и `Marlin/src/lcd/menu/`. Прямые правки общих
   файлов оборачивайте в `FB_G6_*`-флаги, чтобы upstream-мержи оставались
   возможными.
4. **WiFi** — `Marlin/src/module/wifi/`, протокол MKS. Не менять формат
   обмена без проверки с модулем.
5. **Версия** — `Marlin/Version.h`: `SHORT_BUILD_VERSION` зависит от
   `TEMP_SENSOR_0` (5 → 104GT, 13 → 3950). При изменении версии обновляйте обе
   ветки `#if/#elif`.

## 7. Соглашения по коду

Базовый стандарт — [Marlin Coding Standards](https://marlinfw.org/docs/development/coding_standards.html).
Дополнительно:

- отступ — **2 пробела**, LF, финальный перевод строки (`.editorconfig`);
- имена функций и переменных — `snake_case` (`calculate_trapezoid_for_block`),
  классы — `PascalCase`, макросы — `SCREAMING_SNAKE_CASE`, структуры — `_t`;
- условия компиляции — только через `ENABLED()`, `DISABLED()`, `ANY()`,
  `EITHER()`, `BOTH()`, `TERN_()`; не писать `#ifdef` там, где есть
  функциональный макрос;
- строки в прошивке — через `F("...")`, таблицы во flash — через `PROGMEM`;
- фиксированные типы (`uint8_t`, `int32_t`, `const_float_t`), а не `int`/`float`
  там, где важен размер;
- никаких `new`/`delete` и динамического выделения в рантайме в новом коде:
  память статическая, буферы фиксированного размера (исключение — сортировка SD
  при `SDSORT_DYNAMIC_RAM`);
- ISR и `stepper`-код — минимальный, без блокировок, `Serial.print` и
  длинных вычислений;
- комментарии — только «почему», а не «что»; на английском, как в остальном
  коде;
- не добавлять новые `.cpp` в `src/`, не проверив `default_src_filter` в
  `platformio.ini`: файл может не попасть в сборку.

### Стиль Clean Code

В проекте подключён набор скиллов `.opencode/skills/` (адаптация
[clean-code-skills](https://github.com/ertugrul-dmr/clean-code-skills) под C++).
Используйте их при написании и ревью кода:

| Скилл | Когда |
| --- | --- |
| `cpp-clean-code` | полный каталог правил, обзор кода |
| `boy-scout` | любое изменение существующего кода |
| `clean-names` | имена переменных, функций, классов |
| `clean-functions` | разбиение и рефакторинг функций |
| `clean-comments` | комментарии и doc-блоки |
| `clean-general` | DRY, магические числа, дублирование |
| `clean-tests` | тесты и проверки |

Правила скиллов не отменяют стиль Marlin: при конфликте побеждает стиль
репозитория (например, `F()`-строки и `ENABLED()` важнее «чистого» C++).

## 8. Spec Kit: разработка по спецификации

В проекте подключён **Spec Kit** (spec-driven development): сначала
спецификация и план, потом код. Конституция проекта —
`.specify/memory/constitution.md`, она задаёт принципы, которым ДОЛЖНЫ
соответствовать план, задачи и реализация.

### Команды

| Команда | Когда |
| --- | --- |
| `/speckit.constitution` | создать/изменить конституцию проекта |
| `/speckit.specify` | новая фича: `specs/NNN-name/spec.md` |
| `/speckit.clarify` | уточнить неоднозначные требования в спеке |
| `/speckit.plan` | технический план: `plan.md` |
| `/speckit.tasks` | разбить план на задачи: `tasks.md` |
| `/speckit.analyze` | проверить согласованность спеки, плана и задач |
| `/speckit.checklist` | чек-лист качества для области изменений |
| `/speckit.implement` | выполнить задачи по `tasks.md` |
| `/speckit.converge` | свести расхождения артефактов и кода |
| `/speckit.taskstoissues` | выгрузить задачи в GitHub Issues |

### Цикл bugfix

Дефект, найденный после или во время реализации, проходит отдельный цикл:

1. `/speckit.bugfix.report` — зафиксировать баг в
   `specs/{feature}/bugs/BUG-NNN.md` и проследить его до требований, плана и
   задач.
2. `/speckit.bugfix.patch` — точечно обновить `spec.md`, `plan.md`, `tasks.md`
   (добавить требования, переоткрыть ложно выполненные задачи).
3. `/speckit.bugfix.verify` — проверить согласованность артефактов
   (команда только читает файлы).
4. `/speckit.implement` — внести исправление в код.
5. `/speckit.bugfix.switch` — переключиться на контекст другой фичи.

Правила bugfix: не перегенерировать артефакты с нуля, не удалять содержимое
(устаревшее — зачёркивать), переоткрывать задачи, а не удалять их, и всегда
указывать `BUG-NNN` и дату в заметке об изменении.

### Правила работы со Spec Kit

- Вывод команд — на русском; артефакты (`spec.md`, `plan.md`, `tasks.md`) —
  тоже на русском, код и идентификаторы — по правилам раздела 7.
- План и задачи ДОЛЖНЫ ссылаться на принципы конституции; если принцип
  нарушается, это обосновывается явно.
- Приоритет при расхождении: требования пользователя → конституция →
  `AGENTS.md`.
- `specs/` коммитится вместе с кодом; `.specify/feature.json` — локальный
  указатель и не коммитится (см. `.specify/.gitignore`).
- Скрипты Spec Kit — Python: `python3 .specify/scripts/python/...`; запускать
  из корня репозитория.
- Если фича меняет прошивку, её план ДОЛЖЕН включать проверки из раздела 4
  (сборка `mks_robin_nano_v3`, варианты термисторов, BLTouch, форматирование).

## 9. Чего не делать

- **Не отключать защиты**: thermal protection, watchdog, sanity checks,
  software endstops — только осознанно и с объяснением в PR.
- **Не менять карту пинов** без datasheet платы и проверки на железе.
- **Не переписывать `Configuration.h` целиком** и не переносить настройки в
  `config.ini` без причины: конфиг — источник истины для этой прошивки.
- **Не коммитить** `.pio/`, `.npm/`, локальный `Marlin/build_params.h`,
  `firmware.bin`, артефакты IDE.
- **Не запускать** `make tests-single-ci` / `GIT_RESET_HARD` на рабочей копии.
- **Не форматировать** чужие файлы и не делать массовых переименований в
  рамках несвязанной задачи.
- **Не удалять** маркеры `FB_G6_*` и комментарии о совместимости с апстримом.
- **Не добавлять** зависимости в `lib_deps` без проверки размера прошивки:
  у STM32F407 ограниченная flash/CCM.
- **Не менять** протокол MKS WiFi и формат DWIN/TFT-команд без проверки на
  устройстве.

## 10. Git и коммиты

- Ветка по умолчанию — `2.1.2`; не пушить напрямую в неё без необходимости.
- Сообщение коммита: повелительное наклонение, до 72 символов в первой строке
  (`Add custom tramming screen`, `Fix FAN_PWM frequency`).
- Один коммит — одна логическая правка. Не смешивайте форматирование и
  поведение.
- В PR указывайте: что проверено на железе, какой вариант `build_params.h`
  использовался, нужна ли переинициализация EEPROM.
- Секреты (WiFi-пароли, токены) в код и конфиги не коммитить.

## 11. Быстрый чек-лист перед завершением задачи

- [ ] `pio run -e mks_robin_nano_v3` собирается без новых warning'ов.
- [ ] Проверены оба варианта термистора, если правка затрагивает `Version.h`
      или `TEMP_SENSOR_0`.
- [ ] Проверена сборка с `FB_G6_BLTOUCH` и без, если правка в probing/homing.
- [ ] Изменённые файлы отформатированы `buildroot/bin/format_code`.
- [ ] В диффе нет `.pio/`, `.npm/`, `build_params.h`, бинарников.
- [ ] Кастомный код обёрнут в `FB_G6_*`-флаг, если он не должен попадать в
      апстрим-путь.
- [ ] Артефакты Spec Kit (`spec.md`, `plan.md`, `tasks.md`) обновлены и
      согласованы; при дефекте создан `bugs/BUG-NNN.md` и пройден
      `/speckit.bugfix.verify`.
- [ ] Поведение на принтере описано в PR (что проверено, на какой прошивке).
