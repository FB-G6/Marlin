# Contract: раскладка журнала в EEPROM

**Feature**: `001-print-session-diagnostics` | **Версия контракта**: 1

Контракт описывает энергонезависимый формат журнала остановок. Любое изменение
раскладки — новая `version` и новая область (см. «Совместимость»).

## 1. Область

```text
MARLIN_EEPROM_SIZE        = 0x1000 (4096) на MKS Robin Nano V3 (I2C_EEPROM)
FB_G6_PRINT_JOURNAL_REGION= 768 (0x300)
PRINT_JOURNAL_ADDR        = MARLIN_EEPROM_SIZE - FB_G6_PRINT_JOURNAL_REGION
                          = 3328 (0xD00)
```

```text
0x0000 .. 0x003F   резерв (print stats, выключены)
0x0040 .. 0x0063   резерв
0x0064 .. 0x0???   SettingsData (MarlinSettings), начало = EEPROM_OFFSET = 100
...
0x0D00 .. 0x0D0B   JournalHeader                     (12 Б)
0x0D0C .. 0x0ECB   StopEvent[16]                     (448 Б)
0x0ECC .. 0x0F0B   ActiveSession[2]                  (64 Б)
0x0F0C .. 0x0FFF   запас (244 Б, нули, не читается)
```

Граница `SettingsData` проверяется на этапе компиляции:

```cpp
// module/settings.cpp, под #if ENABLED(FB_G6_PRINT_JOURNAL)
static_assert(EEPROM_OFFSET + sizeof(SettingsData) <= PRINT_JOURNAL_ADDR,
              "FB_G6 print journal overlaps EEPROM settings.");
```

## 2. `JournalHeader` — 12 байт, смещение `+0x00`

| Смещение | Размер | Поле | Тип | Значение |
| --- | --- | --- | --- | --- |
| `0` | 4 | `magic` | `uint32_t` LE | `0x314A4246` (`"FBJ1"`) |
| `4` | 1 | `version` | `uint8_t` | `1` |
| `5` | 1 | `slots` | `uint8_t` | `FB_G6_PRINT_JOURNAL_SLOTS` (16) |
| `6` | 2 | `head` | `uint16_t` LE | следующая запись, `0..slots-1` |
| `8` | 2 | `count` | `uint16_t` LE | заполнено, `0..slots` |
| `10` | 2 | `crc16` | `uint16_t` LE | `crc16()` по байтам `0..9` |

`crc16()` — штатная функция Marlin (`Marlin/src/libs/crc16.h`,
реализация `libs/crc16.cpp`, CRC-16/CCITT-FALSE: poly `0x1021`, init `0xFFFF`).

## 3. `StopEvent` — 28 байт, `slots * 28`, смещение `+0x0C`

| Смещение | Размер | Поле | Тип | Правило |
| --- | --- | --- | --- | --- |
| `0` | 4 | `seq` | `uint32_t` LE | `0` = слот пуст |
| `4` | 4 | `duration_s` | `uint32_t` LE | `0..999999` |
| `8` | 1 | `percent` | `uint8_t` | `0..100`, `0xFF` = неизвестно |
| `9` | 1 | `cause` | `uint8_t` | `StopCause` |
| `10` | 1 | `source` | `uint8_t` | `StopSource` |
| `11` | 1 | `stage` | `uint8_t` | `SessionStage` |
| `12` | 1 | `flags` | `uint8_t` | `StopFlags` |
| `13` | 1 | `crc8` | `uint8_t` | CRC8 по байтам `0..12` и `14..27` |
| `14` | 13 | `file` | `char[13]` | DOS 8.3, NUL-дополнено |
| `27` | 1 | `reserved` | `uint8_t` | `0` |

CRC8: poly `0x07`, init `0x00`, без отражения, без финального XOR
(CRC-8/SMBUS). Считается по 27 байтам: `0..12` и `14..27` (без байта `crc8`).

## 4. `ActiveSession` — 32 байта, 2 слота, смещение `+0x0C+448`

| Смещение | Размер | Поле | Тип | Правило |
| --- | --- | --- | --- | --- |
| `0` | 4 | `magic` | `uint32_t` LE | `0x31415346` (`"FSA1"`) |
| `4` | 4 | `seq` | `uint32_t` LE | возрастает на каждую запись; чётность = номер слота |
| `8` | 4 | `elapsed_s` | `uint32_t` LE | контрольная длительность |
| `12` | 1 | `percent` | `uint8_t` | `0xFF` = неизвестно |
| `13` | 1 | `stage` | `uint8_t` | `SessionStage` |
| `14` | 1 | `active` | `uint8_t` | `1` — сессия идёт, `0` — закрыта |
| `15` | 1 | `crc8` | `uint8_t` | CRC8 (poly `0x07`, init `0x00`) по байтам `0..14` и `16..31` |
| `16` | 13 | `file` | `char[13]` | DOS 8.3 |
| `29` | 3 | `reserved` | `uint8_t[3]` | `0` |

Инвариант чередования: запись выполняется в слот `seq & 1`, `seq`
предыдущего слота не меняется → при обрыве записи остаётся валидная копия с
меньшим `seq`.

## 5. Протокол доступа

```text
init():
  persistentStore.access_start()
    read(PRINT_JOURNAL_ADDR, header, 12)
    read(PRINT_JOURNAL_ADDR+12, slots, slots*28)
    read(PRINT_JOURNAL_ADDR+12+slots*28, active, 64)
  persistentStore.access_finish()
  validate/repair (см. §6)

append(event):
  persistentStore.access_start()
    write(PRINT_JOURNAL_ADDR + 12 + head*28, &event, 28)   // сначала слот
    header.head = (head + 1) % slots; header.count = min(count+1, slots)
    write(PRINT_JOURNAL_ADDR, &header, 12)                 // потом заголовок
  persistentStore.access_finish()

checkpoint(session):
  persistentStore.access_start()
    slot = session.seq & 1
    write(PRINT_JOURNAL_ADDR+12+slots*28 + slot*32, &session, 32)
  persistentStore.access_finish()
```

Порядок «слот → заголовок» гарантирует, что при обрыве питания заголовок
никогда не ссылается на недописанный слот; при обратном порядке возможна
потеря только что записанного события.

Ограничения:
- `persistentStore.write_data` пишет только отличающиеся байты
  (`HAL/STM32/eeprom_wired.cpp:52`), поэтому стоимость записи пропорциональна
  числу изменённых байт;
- `write_data` возвращает `true` при ошибке: событие считается записанным
  только при `false`; иначе в RAM ставится `FLAG_UNRECORDED_EMERGENCY`
  (для аварии) или событие просто теряется (для штатных причин — журнал не
  должен ломать печать). Если слот записан, а заголовок — нет, слот помечается
  недействительным (`crc8 = 0`), чтобы сканирование его не воскресило;
- запись запрещена в ISR шагового контура и в `stepper`/`planner` критических
  участках; `kill()`/`loud_kill()` работают в нормальном контексте, но запись
  EEPROM при аварии выполняется best-effort из-за риска просадки питания:
  причина сначала фиксируется в `noinit`-записи, затем пишется событие; если
  запись не удалась, `init()` синтезирует событие при следующей загрузке
  (см. research R7).

## 6. Валидация и восстановление

| Проверка | Не прошла | Действие |
| --- | --- | --- |
| `magic == 0x314A4246` | да | журнал пуст, `LOG` в serial при `DEBUG_LEVELING_FEATURE`/`M111 S1` (без ошибки) |
| `version == 1`, `slots == CONFIG` | да | журнал пуст (несовместимая раскладка) |
| `head < slots`, `count <= slots` | да | восстановить сканированием |
| `crc16` заголовка | да | восстановить сканированием |
| `crc8` слота | да | слот пропустить |
| `crc8` активной сессии | да | взять второй слот; если оба битые → активной сессии нет |

Восстановление заголовка: `count` = число валидных слотов, `head` =
`(индекс слота с максимальным seq + 1) % slots`; если валидных нет —
`count = 0`, `head = 0`.

Восстановление **никогда** не вызывает `settings.reset()`, `settings.save()`,
`M502` или `M500` и не пишет в область `SettingsData`.

## 7. Совместимость

- Журнал не участвует в `M500`/`M501`/`M502`/`M503`, `EEPROM_VERSION` и
  `SettingsData` не меняются → `Initialize EEPROM` не требуется (FR-012).
- `EEPROM_INIT_NOW` и смена `build_hash` при пересборке перезаписывают только
  `SettingsData`; журнал сохраняется.
- Изменение размера/раскладки: увеличить `version` **и**
  `FB_G6_PRINT_JOURNAL_REGION` так, чтобы новая область не пересекалась со
  старой; старые данные при этом игнорируются, настройки не затрагиваются.
- Если `EEPROM_OFFSET + sizeof(SettingsData) > PRINT_JOURNAL_ADDR` или
  `EEPROM_SETTINGS` выключен — `#error` в `inc/SanityCheck.h`, а не тихая
  деградация.
