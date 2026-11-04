---
name: clean-general
description: Use when reviewing or improving general code quality in this Marlin firmware repository. Enforces DRY, clear intent, named constants, single responsibility, and boundary handling.
---

# Общие принципы Clean Code

## Ключевые правила

### G5: DRY — не повторяйся

У каждого знания одно авторитетное представление.

```cpp
// Плохо: одно и то же знание в двух местах
constexpr float X_MAX = 235.0f;
constexpr float Y_MAX = 235.0f;
if (x > 235.0f || y > 235.0f) { /* ... */ }

// Хорошо: единый источник
constexpr float X_MAX = 235.0f;
constexpr float Y_MAX = 235.0f;
inline bool position_in_bounds(const float x, const float y) {
  return x >= X_MIN_POS && x <= X_MAX && y >= Y_MIN_POS && y <= Y_MAX;
}
```

В Marlin границы уже описаны константами (`X_BED_SIZE`, `X_MAX_POS`,
`X_MIN_POS`) — не дублируй числа.

### G16: Не прячь намерение

Не хитри. Будь понятен.

```cpp
// Плохо: что здесь происходит?
return ((x & 0x0F) << 4) | (y & 0x0F);

// Хорошо: намерение видно
return pack_coordinates(x, y);
```

### G23: Полиморфизм вместо if/else

```cpp
// Плохо: цепочка будет расти с каждым новым типом
float axis_steps(const uint8_t axis) {
  if (axis == X_AXIS) return planner.settings.axis_steps_per_mm[X_AXIS];
  else if (axis == Y_AXIS) return planner.settings.axis_steps_per_mm[Y_AXIS];
  // ...
}

// Хорошо: массив по индексу оси
return planner.settings.axis_steps_per_mm[axis];
```

На MCU виртуальные функции дороги по flash/RAM. «Полиморфизм» здесь — это
таблицы, индексы, `switch` по `enum class` с покрытием всех значений, а не
обязательно `virtual`. Главное — не плодить ветвления по типу в вызывающем коде.

### G25: Именованные константы вместо магических чисел

```cpp
// Плохо
if (elapsed > 86400) { /* ... */ }

// Хорошо
constexpr uint32_t SECONDS_PER_DAY = 86400;
if (elapsed_seconds > SECONDS_PER_DAY) { /* ... */ }
```

В Marlin константы конфигурации приходят из `Configuration.h`; локальные
константы — `constexpr` в `.cpp` или `static constexpr` в классе.

### G30: Функция делает одно дело

Если из функции можно извлечь ещё одну функцию — она делает больше одного дела.

### G36: Law of Demeter

```cpp
// Плохо: цепочка через несколько объектов
const float z = planner.moves[0].block->position.z;

// Хорошо: один уровень
const float z = planner.get_move_z(0);
```

## Проверочный список

При ревью сгенерированного кода проверь:

- [ ] нет дублирования (G5);
- [ ] нет магических чисел и строк (G25);
- [ ] намерение не скрыто (G16);
- [ ] нет длинных цепочек `if/else` по типу (G23);
- [ ] функция делает одно дело (G30);
- [ ] нет нарушений Law of Demeter (G36);
- [ ] обработаны граничные условия (G3);
- [ ] мёртвый код удалён (G9);
- [ ] нет отрицательных условий без нужды (G29);
- [ ] временны́е зависимости явные (G31).

## Специфика прошивки

- **G4:** не обходить thermal protection, watchdog, software endstops и
  `SanityCheck.h`. Это не «лишний код», а защита железа.
- **G12:** не тащить в код отладочные `SERIAL_ECHO` без флага
  (`DEBUG_LEVELING_FEATURE`, `MARLIN_DEBUG` и т.п.).
- **G35:** конфигурация — в `Configuration.h`/`Configuration_adv.h`, а не
  разбросана по `.cpp`.
- **G11:** одинаковые задачи решай одинаково во всех HAL/платах.

Источник: [clean-code-skills](https://github.com/ertugrul-dmr/clean-code-skills) (MIT), адаптация под C++/Marlin.
