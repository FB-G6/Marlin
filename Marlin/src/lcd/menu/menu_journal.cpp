/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

//
// Stop journal menu (FB-G6)
//

#include "../../inc/MarlinConfigPre.h"

#if BOTH(HAS_MARLINUI_MENU, FB_G6_PRINT_JOURNAL)

#include "menu_item.h"
#include "../../feature/print_session.h"

void menu_journal() {
  START_MENU();
  BACK_ITEM(MSG_MAIN);

  const uint8_t total = print_session.count();
  if (!total)
    STATIC_ITEM(MSG_STOP_JOURNAL_EMPTY);
  else
    for (uint8_t i = 0; i < total; i++) {
      const StopEvent * const ev = print_session.event_at(i);
      if (!ev) continue;

      // Longest RU line: "#16 Сброс: сторожевой таймер | Прошивка | ~HH:MM:SS"
      char line[96], time[16];
      print_session.format_duration(time, ev->duration_s, ev->flags & FLAG_ESTIMATED_DURATION);
      sprintf_P(line, PSTR("#%u %s | %s | %s"),
        uint16_t(total - i),
        FTOP(print_session.cause_text(StopCause(ev->cause))),
        FTOP(print_session.source_text(StopSource(ev->source))),
        time
      );
      STATIC_ITEM_F(F(""), SS_DEFAULT, line);

      char progress[8];
      if (ev->percent == 0xFF) strcpy_P(progress, PSTR("-"));
      else sprintf_P(progress, PSTR("%u%%"), ev->percent);

      sprintf_P(line, PSTR("%s | %s"), ev->file[0] ? ev->file : "-", progress);
      STATIC_ITEM_F(F(""), SS_DEFAULT, line);
    }

  END_MENU();
}

#endif // HAS_MARLINUI_MENU && FB_G6_PRINT_JOURNAL
