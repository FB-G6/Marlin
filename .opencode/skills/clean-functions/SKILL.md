---
name: clean-functions
description: Use when writing, splitting, or refactoring functions and methods in this Marlin firmware repository. Enforces Clean Code function rules F1-F4 and embedded C++ constraints.
---

# Clean Functions

## F1: Не больше трёх аргументов

Больше трёх аргументов — признак того, что функция делает слишком много или
нужна структура.

```cpp
// Плохо: 7 аргументов
void draw_label(const char *text, int16_t x, int16_t y,
                uint16_t color, uint8_t font, bool centered, bool inverted);

// Хорошо: параметры сгруппированы
struct LabelStyle {
  uint16_t color;
  uint8_t font;
  bool centered;
  bool inverted;
};

void draw_label(const char *text, const int16_t x, const int16_t y,
                const LabelStyle &style);
```

Для координат в Marlin уже есть `xy_int_t`, `xyze_float_t`, `xy_pos_t` —
используй их вместо пары аргументов `x, y`.

## F2: Никаких выходных аргументов

Не меняй аргументы как побочный эффект, если можно вернуть значение.

```cpp
// Плохо: результат пишется в аргумент
void compute_steps(float distance, int32_t *steps);

// Хорошо: результат возвращается
int32_t steps_for_distance(const float distance);

// Если значений несколько — вернуть структуру
struct StepCounts { int32_t x, y, z, e; };
StepCounts steps_for_move(const xyze_float_t &distance);
```

Исключение — буферы, которые заполняются по контракту (например,
`format_bytes(char *buf, size_t size, ...)`); такое должно быть явно видно из
имени и комментария.

## F3: Никаких булевых флагов-аргументов

Флаг означает, что функция делает как минимум два разных дела.

```cpp
// Плохо
void home_axes(const bool raise_z);

// Хорошо: две явные операции
void home_axes();
void home_axes_with_z_raise();
```

В Marlin булевы параметры встречаются, но для нового кода предпочитай отдельные
функции или enum-класс с понятными значениями.

## F4: Мёртвые функции удалять

Если функция не вызывается — удали. Git сохранит историю. Не оставляй
«на всякий случай» и закомментированные реализации.

## Дополнительно для прошивки

- **Одно дело на функцию (G30).** Если можешь выделить ещё одну функцию — эта
  делает больше одного дела.
- **Один уровень абстракции (G34).** Не смешивай работу с железом и
  форматирование строки в одной функции.
- **Ранние возвраты вместо глубокой вложенности.**
- **Функции в ISR — короткие и без блокирующих вызовов.** Никаких
  `Serial.print`, `delay()`, ожидания периферии.
- **Никаких `new`/`delete` и больших буферов на стеке (CPP1, CPP6).**
- **Осторожно с `static` внутри функции в ISR-контексте:** состояние должно
  быть явным (G31).

```cpp
// Плохо: смешаны проверка, вычисление и печать
void handle_temperature() {
  if (thermalManager.hotend_maxtemp[0] && current_temp > maxtemp) {
    SERIAL_ERROR_MSG("Heating failed");
    kill();
  }
  // ... ещё 40 строк ...
}

// Хорошо: каждое дело — своя функция
bool hotend_overheated(const uint8_t index);
void report_heating_failure(const uint8_t index);
```

## Быстрая таблица

| Правило | Суть |
| --- | --- |
| F1 | Максимум 3 аргумента, дальше — структура |
| F2 | Возвращать значение, а не писать в аргумент |
| F3 | Без булевых флагов, split the function |
| F4 | Мёртвые функции удалять |

Источник: [clean-code-skills](https://github.com/ertugrul-dmr/clean-code-skills) (MIT), адаптация под C++/Marlin.
