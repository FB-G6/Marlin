---
name: clean-names
description: Use when naming or renaming variables, functions, classes, structs, macros, or files in this Marlin firmware repository. Enforces Clean Code naming rules N1-N7 and Marlin naming conventions.
---

# Clean Names

## N1: Имя раскрывает намерение

Если для понимания имени нужен комментарий — имя плохое.

```cpp
// Плохо: что такое d?
const uint32_t d = 86400;

// Хорошо: смысл виден
constexpr uint32_t SECONDS_PER_DAY = 86400;

// Плохо: что делает функция?
void proc(float *v, uint8_t n);

// Хорошо: намерение ясно
void scale_feedrate(float *feedrates, const uint8_t count);
```

## N2: Имя на своём уровне абстракции

Имя не должно выдавать реализацию, если функция — часть более высокого уровня.

```cpp
// Плохо: слишком про реализацию
uint16_t get_array_index_of_hotend_by_number(const uint8_t index);

// Хорошо: уровень предметной области
hotend_t &get_hotend(const uint8_t index);
```

## N3: Стандартная терминология

Используй термины предметной области и устоявшиеся паттерны. В прошивке это
`stepper`, `planner`, `block`, `segment`, `homing`, `probing`, `babystep`,
`trapezoid`, `lookahead`. Не изобретай синонимы.

```cpp
// Плохо: самодельный термин
void move_motor_smoothly(...);

// Хорошо: термин Marlin
void planner_buffer_line(...);
```

## N4: Однозначные имена

```cpp
// Плохо: что во что переименовываем?
void rename(const char *source, const char *target);

// Хорошо
void rename_file(const char *old_path, const char *new_path);
```

## N5: Длина имени соответствует области видимости

```cpp
// Хорошо: короткое имя в крошечной области видимости
for (uint8_t i = 0; i < count; i++) sum += values[i];

// Хорошо: длинное имя для глобальной константы
constexpr uint16_t MAX_RETRY_ATTEMPTS_BEFORE_FAILURE = 5;

// Плохо: короткое имя на уровне модуля
constexpr uint16_t MAX = 5;
```

## N6: Без кодирования типа в имени

```cpp
// Плохо: венгерская нотация
char *str_name;
float *arr_values;
uint8_t n_count;
class CStepper;

// Хорошо
char *name;
float *values;
uint8_t count;
class Stepper;
```

## N7: Имя описывает побочные эффекты

```cpp
// Плохо: имя не говорит, что функция создаёт запись
Settings &get_settings(const char *key);

// Хорошо: поведение видно из имени
Settings &get_or_create_settings(const char *key);
```

## Конвенции Marlin

| Сущность | Стиль | Пример |
| --- | --- | --- |
| Функция, переменная | `snake_case` | `calculate_trapezoid_for_block` |
| Класс | `PascalCase` | `Planner`, `Stepper` |
| Структура | `snake_case` + `_t` | `block_t`, `xyze_float_t` |
| Макрос конфигурации | `SCREAMING_SNAKE_CASE` | `DEFAULT_MAX_FEEDRATE` |
| Приватный член | без префикса, по смыслу | `block_buffer_head` |
| Файл | `snake_case` / имя класса | `planner.cpp`, `MarlinCore.cpp` |

Не вводить префиксы `m_`, `g_`, `s_` для членов и глобалов: в Marlin их нет.

## Быстрая таблица

| Правило | Принцип | Пример |
| --- | --- | --- |
| N1 | Раскрывает намерение | `SECONDS_PER_DAY`, не `d` |
| N2 | Уровень абстракции | `get_hotend()`, не `get_array_index…` |
| N3 | Стандартные термины | `homing`, `probing`, `babystep` |
| N4 | Однозначность | `rename_file(old_path, new_path)` |
| N5 | Длина = область видимости | `i` в цикле, длинно — для глобалов |
| N6 | Без кодирования типа | `values`, не `arr_values` |
| N7 | Побочные эффекты в имени | `get_or_create_settings()` |

Источник: [clean-code-skills](https://github.com/ertugrul-dmr/clean-code-skills) (MIT), адаптация под C++/Marlin.
