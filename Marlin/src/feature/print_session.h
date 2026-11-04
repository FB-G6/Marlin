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
#pragma once

/**
 * feature/print_session.h - Print session diagnostics (FB-G6)
 *
 * Records why and how a print session stopped and keeps the last events in
 * a dedicated EEPROM area. The stop behavior itself is NOT changed:
 * the observable behavior of every stop kind is documented in
 * specs/001-print-session-diagnostics/contracts/stop-reasons.md §3 and must
 * stay identical (FR-006).
 */

#include "../inc/MarlinConfig.h"

#if ENABLED(FB_G6_PRINT_JOURNAL)

#include <stdint.h>

/**
 * Stable reason codes. Values are part of the on-EEPROM contract and must
 * never change; add new reasons inside their range (0x1x, 0x2x, ...).
 */
enum StopCause : uint8_t {
  CAUSE_COMPLETED         = 0x00, // File finished normally
  CAUSE_ABORT_LOCAL_UI    = 0x10, // Cancelled from the screen or the menu
  CAUSE_ABORT_NETWORK     = 0x11, // Cancelled by the network (M26 over MKS WiFi)
  CAUSE_ABORT_HOST        = 0x12, // Cancelled from the console (M524)
  CAUSE_MEDIA_ERROR       = 0x20, // SD card removed or read failure
  CAUSE_RESET_WATCHDOG    = 0x30, // Reset by the watchdog
  CAUSE_RESET_POWER       = 0x31, // Power on / brownout
  CAUSE_RESET_SOFTWARE    = 0x32, // Software or reset button
  CAUSE_EMERGENCY_THERMAL = 0x40, // Thermal protection triggered kill()
  CAUSE_EMERGENCY_M112    = 0x41, // M112 emergency stop
  CAUSE_EMERGENCY_OTHER   = 0x48, // Any other kill()
  CAUSE_INTERNAL_STOP     = 0x50, // stop() without kill()
  CAUSE_UNKNOWN           = 0x7F  // No source information available
};

enum StopSource : uint8_t {
  SOURCE_UNKNOWN   = 0x00,
  SOURCE_LOCAL_UI  = 0x01,
  SOURCE_NETWORK   = 0x02,
  SOURCE_HOST      = 0x03,
  SOURCE_MEDIA     = 0x04,
  SOURCE_FIRMWARE  = 0x05
};

enum SessionStage : uint8_t {
  STAGE_NONE            = 0x00,
  STAGE_PRINTING        = 0x01,
  STAGE_PAUSED          = 0x02,
  STAGE_FILAMENT_CHANGE = 0x03,
  STAGE_FINISHING       = 0x04
};

enum StopFlags : uint8_t {
  FLAG_NONE                  = 0x00,
  FLAG_EMERGENCY             = _BV(0),
  FLAG_ESTIMATED_DURATION    = _BV(1),
  FLAG_UNRECORDED_EMERGENCY  = _BV(2),
  FLAG_EMERGENCY_DETAIL_LOST = _BV(3)
};

#pragma pack(push, 1)

/** One journal entry, 28 bytes (contract: journal-storage.md §3). */
struct StopEvent {
  uint32_t seq;         // Monotonic number, 0 = empty slot
  uint32_t duration_s;  // Seconds since the job start, 0..999999
  uint8_t  percent;     // card.percentDone(), 0xFF = unknown
  uint8_t  cause;       // StopCause
  uint8_t  source;      // StopSource
  uint8_t  stage;       // SessionStage at the stop moment
  uint8_t  flags;       // StopFlags
  uint8_t  crc8;        // CRC8 over bytes 0..12 and 14..27
  char     file[13];    // DOS 8.3 name, NUL padded
  uint8_t  reserved;    // 0
};

/** Journal header, 12 bytes (contract: journal-storage.md §2). */
struct JournalHeader {
  uint32_t magic;       // 0x314A4246 ("FBJ1")
  uint8_t  version;     // 1
  uint8_t  slots;       // FB_G6_PRINT_JOURNAL_SLOTS
  uint16_t head;        // Index of the next write
  uint16_t count;       // Number of valid entries
  uint16_t crc16;       // Marlin crc16() over bytes 0..9
};

/** Active session slot, 32 bytes; two slots alternate (contract §4). */
struct ActiveSession {
  uint32_t magic;       // 0x31415346 ("FSA1")
  uint32_t seq;         // Grows on every write, parity selects the slot
  uint32_t elapsed_s;   // Last known duration
  uint8_t  percent;     // Last known progress, 0xFF = unknown
  uint8_t  stage;       // SessionStage
  uint8_t  active;      // 1 = session running, 0 = cleanly closed
  uint8_t  crc8;        // CRC8 over bytes 0..14 and 16..31
  char     file[13];    // DOS 8.3 name
  uint8_t  reserved[3]; // 0
};

/**
 * Emergency record kept in .noinit RAM. It survives a software reset and is
 * consumed on the next boot; a power loss destroys it on purpose.
 */
struct EmergencyRecord {
  uint32_t magic;       // 0x31455246 ("FRE1")
  uint8_t  cause;       // StopCause
  uint8_t  source;      // StopSource
  uint8_t  crc8;        // CRC8 over bytes 0..5 and 7..15
  uint8_t  reserved[9]; // 0
};

#pragma pack(pop)

static_assert(sizeof(StopEvent) == 28, "StopEvent must be 28 bytes.");
static_assert(sizeof(JournalHeader) == 12, "JournalHeader must be 12 bytes.");
static_assert(sizeof(ActiveSession) == 32, "ActiveSession must be 32 bytes.");
static_assert(sizeof(EmergencyRecord) == 16, "EmergencyRecord must be 16 bytes.");

/** Dedicated EEPROM area at the very end of the EEPROM (contract §1). */
constexpr uint16_t PRINT_JOURNAL_ADDR        = MARLIN_EEPROM_SIZE - FB_G6_PRINT_JOURNAL_REGION;
constexpr uint16_t PRINT_JOURNAL_HEADER_ADDR = PRINT_JOURNAL_ADDR;
constexpr uint16_t PRINT_JOURNAL_SLOTS_ADDR  = PRINT_JOURNAL_HEADER_ADDR + sizeof(JournalHeader);
constexpr uint16_t PRINT_JOURNAL_ACTIVE_ADDR = PRINT_JOURNAL_SLOTS_ADDR + FB_G6_PRINT_JOURNAL_SLOTS * sizeof(StopEvent);

constexpr uint32_t JOURNAL_MAGIC          = 0x314A4246UL; // "FBJ1"
constexpr uint32_t ACTIVE_SESSION_MAGIC   = 0x31415346UL; // "FSA1"
constexpr uint32_t EMERGENCY_RECORD_MAGIC = 0x31455246UL; // "FRE1"
constexpr uint8_t  JOURNAL_VERSION        = 1;

class PrintSession {
public:
  // Boot: remember the MCU reset source before the HAL clears it.
  static void set_boot_reset(const uint8_t mcu);

  // Load and repair the journal, then synthesize a reset/emergency event.
  static void init();

  // Called from idle(): checkpoints and the deferred stop reason screen.
  static void task();

  // Stop source bookkeeping for the single abort funnel.
  static void note_source(const StopSource src);
  static StopSource take_source();
  // An abort without a source tag maps to CAUSE_UNKNOWN but is still
  // recorded with duration, percent, stage and file name (FR-007).
  static StopCause cause_for_source(const StopSource src);

  // Emergency subtype bookkeeping. First specific subtype wins.
  static void note_emergency(const StopCause cause);
  static StopCause emergency_cause();

  // Called from kill() after the heaters are off: writes the .noinit record
  // and makes a best-effort attempt to store the event in the journal.
  static void commit_emergency();

  // Session lifecycle.
  static void start_session();
  static void checkpoint(const bool force=false);
  static void end_session(const StopCause cause, const StopSource source);

  // Consecutive SD read errors (media degradation detection).
  static bool note_read_error();
  static void clear_read_errors();

  // Journal reading, index 0 is the newest event.
  static const StopEvent& last_event();
  static const StopEvent* event_at(const uint8_t index);
  static uint8_t count();

  // Localized texts, shared by the modal screen and the journal menu.
  static FSTR_P cause_text(const StopCause cause);
  static FSTR_P source_text(const StopSource source);

  // "HH:MM:SS" with a leading '~' for an estimated duration
  static void format_duration(char * const buffer, const uint32_t seconds, const bool estimated);
};

extern PrintSession print_session;

#endif // FB_G6_PRINT_JOURNAL
