---
name: clean-comments
description: Use when writing, editing, or reviewing comments in this Marlin firmware repository. Enforces Clean Code comment rules C1-C5 and Marlin doxygen/comment conventions.
---

# Clean Comments

## C1: Никаких неуместных сведений

Комментарии не хранят метаданные. Автор, дата, номер тикета, история изменений —
это Git, а не код.

```cpp
// Плохо
// Author: John, Modified: 2024-01-15, Ticket: #123
void home_axes();

// Хорошо: только техническая суть
// Home all axes without raising Z first.
void home_axes();
```

## C2: Устаревшие комментарии удалять сразу

Если комментарий описывает код, которого уже нет или который работает иначе,
удаляй немедленно. Устаревший комментарий хуже отсутствующего: он врёт.

Особенно опасно в этом репозитории: кастомный код FB-G6 обёрнут флагами, и
комментарий «always runs» может стать ложью после изменения `#if`.

## C3: Никаких избыточных комментариев

```cpp
// Плохо: код уже это сказал
i += 1;                    // increment i
stepper.enable();          // enable stepper
block->flag |= BLOCK_BIT;  // set bit

// Хорошо: объясняет ПОЧЕМУ
i += 1;                    // compensate for zero-based display index
```

## C4: Если комментарий нужен — писать хорошо

- подбирать точные слова;
- соблюдать грамматику;
- не растекаться и не пересказывать очевидное;
- быть кратким;
- писать на английском, как в остальном коде Marlin.

Для публичных функций допустим Doxygen-стиль, если он уже используется рядом:

```cpp
/**
 * Apply the trapezoid acceleration profile to a block.
 *
 * \param block  Block to modify; must not be null.
 * \param entry_factor  Entry speed factor in (0, 1].
 */
void calculate_trapezoid_for_block(block_t * const block, const_float_t entry_factor);
```

## C5: Никогда не коммитить закомментированный код

```cpp
// НЕ ДЕЛАТЬ ТАК
// void old_tramming_screen() {
//   ...
// }
```

Кто знает, зачем это здесь и актуально ли? Удаляй. Git помнит всё.

## Где комментарий действительно нужен

- неочевидная аппаратная причина (`FAN_PWM 39.2k -> 125 Hz (fix for gdstime)`);
- ограничение железа/протокола;
- порядок вызовов, который нельзя нарушать (G31);
- единицы измерения и диапазоны, если они не видны из типа;
- почему выбрано именно такое значение (таймаут, коэффициент, задержка).

## Цель

Лучший комментарий — сам код. Если нужно объяснить, **что** делает код,
сначала отрефактори. Комментарий пишется последним и объясняет **почему**.

Источник: [clean-code-skills](https://github.com/ertugrul-dmr/clean-code-skills) (MIT), адаптация под C++/Marlin.
