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

/**
 * feature/print_session.cpp - Print session diagnostics (FB-G6)
 *
 * Layout and validation rules:
 *   specs/001-print-session-diagnostics/contracts/journal-storage.md
 * Stop codes and the unchanged stop behavior:
 *   specs/001-print-session-diagnostics/contracts/stop-reasons.md
 */

#include "../inc/MarlinConfig.h"

#if ENABLED(FB_G6_PRINT_JOURNAL)

#include "print_session.h"

#include <stddef.h>
#include <string.h>

#include "../HAL/shared/eeprom_api.h"
#include "../libs/crc16.h"
#include "../module/planner.h"
#include "../module/printcounter.h"
#include "../sd/cardreader.h"
#include "../MarlinCore.h"
#include "pause.h"

#if HAS_DISPLAY
  #include "../lcd/marlinui.h"
#endif

PrintSession print_session;

namespace {

  constexpr uint8_t  JOURNAL_SLOTS  = FB_G6_PRINT_JOURNAL_SLOTS;
  constexpr uint16_t JOURNAL_SIZE   = sizeof(JournalHeader) + JOURNAL_SLOTS * sizeof(StopEvent) + 2 * sizeof(ActiveSession);
  constexpr uint32_t MAX_DURATION_S = 999999UL;

  static_assert(JOURNAL_SIZE <= FB_G6_PRINT_JOURNAL_REGION, "FB_G6_PRINT_JOURNAL_REGION is too small.");

  JournalHeader header;
  StopEvent     events[JOURNAL_SLOTS];
  ActiveSession active_slots[2];
  ActiveSession session;

  bool journal_ready = false;
  bool session_active = false;
  uint32_t last_seq = 0;

  uint8_t boot_reset = 0;
  uint8_t pending_source = SOURCE_UNKNOWN;

  // 0 means "no emergency subtype recorded yet"
  uint8_t emergency_cause_ram = 0;
  uint8_t emergency_source_ram = SOURCE_UNKNOWN;
  bool emergency_record_valid = false;
  bool emergency_detail_lost = false;
  bool emergency_committed = false;

  bool modal_pending = false;
  uint8_t sd_read_errors = 0;
  millis_t last_checkpoint_ms = 0;

  StopEvent no_event; // All zeroes

  /**
   * Frozen on purpose: survives hal.reboot() and the reset button,
   * destroyed by a power loss (see FLAG_EMERGENCY_DETAIL_LOST).
   */
  #if defined(__arm__)
    __attribute__((section(".noinit"))) EmergencyRecord emergency_record;
  #else
    EmergencyRecord emergency_record;
  #endif

  uint8_t crc8_byte(uint8_t crc, const uint8_t data) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
    return crc;
  }

  uint8_t crc8_block(const uint8_t *data, size_t len, uint8_t crc=0) {
    while (len--) crc = crc8_byte(crc, *data++);
    return crc;
  }

  uint8_t event_crc(const StopEvent &ev) {
    const uint8_t * const p = (const uint8_t *)&ev;
    constexpr uint8_t crc_offset = offsetof(StopEvent, crc8);
    constexpr uint8_t tail = offsetof(StopEvent, file);
    return crc8_block(p + tail, sizeof(StopEvent) - tail, crc8_block(p, crc_offset));
  }

  uint8_t active_crc(const ActiveSession &as) {
    const uint8_t * const p = (const uint8_t *)&as;
    constexpr uint8_t crc_offset = offsetof(ActiveSession, crc8);
    constexpr uint8_t tail = offsetof(ActiveSession, file);
    return crc8_block(p + tail, sizeof(ActiveSession) - tail, crc8_block(p, crc_offset));
  }

  uint8_t emergency_crc(const EmergencyRecord &er) {
    const uint8_t * const p = (const uint8_t *)&er;
    constexpr uint8_t crc_offset = offsetof(EmergencyRecord, crc8);
    constexpr uint8_t tail = offsetof(EmergencyRecord, reserved);
    return crc8_block(p + tail, sizeof(EmergencyRecord) - tail, crc8_block(p, crc_offset));
  }

  uint16_t header_crc(const JournalHeader &jh) {
    constexpr uint8_t crc_offset = offsetof(JournalHeader, crc16);
    uint16_t crc = 0xFFFF;
    crc16(&crc, (const uint8_t *)&jh, crc_offset);
    return crc;
  }

  uint32_t clamp_duration(const uint32_t seconds) {
    return seconds > MAX_DURATION_S ? MAX_DURATION_S : seconds;
  }

  void copy_filename(char * const dst, const char * const src) {
    uint8_t n = 0;
    if (src) while (n < 12 && src[n]) { dst[n] = src[n]; n++; }
    while (n < 13) dst[n++] = '\0';
  }

  bool is_emergency_cause(const StopCause cause) {
    return cause >= CAUSE_EMERGENCY_THERMAL && cause <= CAUSE_EMERGENCY_OTHER;
  }

  bool is_terminal_cause(const uint8_t cause) {
    switch (cause) {
      case CAUSE_COMPLETED:
      case CAUSE_ABORT_LOCAL_UI:
      case CAUSE_ABORT_NETWORK:
      case CAUSE_ABORT_HOST:
      case CAUSE_MEDIA_ERROR:
      case CAUSE_INTERNAL_STOP:
        return true;
      default:
        return is_emergency_cause(StopCause(cause));
    }
  }

  bool is_known_cause(const uint8_t cause) {
    switch (cause) {
      case CAUSE_COMPLETED:
      case CAUSE_ABORT_LOCAL_UI:
      case CAUSE_ABORT_NETWORK:
      case CAUSE_ABORT_HOST:
      case CAUSE_MEDIA_ERROR:
      case CAUSE_RESET_WATCHDOG:
      case CAUSE_RESET_POWER:
      case CAUSE_RESET_SOFTWARE:
      case CAUSE_EMERGENCY_THERMAL:
      case CAUSE_EMERGENCY_M112:
      case CAUSE_EMERGENCY_OTHER:
      case CAUSE_INTERNAL_STOP:
      case CAUSE_UNKNOWN:
        return true;
      default:
        return false;
    }
  }

  uint8_t current_stage() {
    if (printingIsPaused()) return STAGE_PAUSED;
    if (IS_SD_PRINTING()) return STAGE_PRINTING;
    return STAGE_NONE;
  }

  uint8_t current_percent() {
    if (card.flag.sdprintdone) return 100;
    if (card.isFileOpen()) {
      const uint8_t p = card.percentDone();
      return p <= 100 ? p : 0xFF;
    }
    return session.percent;
  }

  StopCause reset_cause(const uint8_t mcu) {
    if (mcu & RST_WATCHDOG) return CAUSE_RESET_WATCHDOG;
    if ((mcu & RST_SOFTWARE) || (mcu & RST_EXTERNAL)) return CAUSE_RESET_SOFTWARE;
    return CAUSE_RESET_POWER; // Power-on, brownout or an unknown flag
  }

  uint32_t next_seq() {
    return last_seq >= 0xFFFFFFFFUL ? 1UL : last_seq + 1;
  }

  bool write_active() {
    session.crc8 = active_crc(session);
    if (!persistentStore.access_start()) return false;
    const uint8_t slot = session.seq & 1;
    int pos = PRINT_JOURNAL_ACTIVE_ADDR + slot * sizeof(ActiveSession);
    const bool err = persistentStore.write_data(pos, (const uint8_t *)&session, sizeof(session));
    persistentStore.access_finish();
    return !err;
  }

  void close_active_session() {
    session.active = 0;
    session.seq++;
    write_active();
  }

  bool load_active_session() {
    bool found = false;
    uint32_t best_seq = 0;
    for (uint8_t i = 0; i < 2; i++) {
      ActiveSession &as = active_slots[i];
      if (as.magic == ACTIVE_SESSION_MAGIC && as.crc8 == active_crc(as)) {
        as.file[12] = '\0';
        if (as.percent > 100 && as.percent != 0xFF) as.percent = 0xFF;
        if (!found || as.seq > best_seq) {
          session = as;
          best_seq = as.seq;
          found = true;
        }
      }
    }
    if (found) session_active = session.active == 1;
    return found;
  }

  bool write_event_slot(const uint8_t index, const StopEvent &ev) {
    int pos = PRINT_JOURNAL_SLOTS_ADDR + index * sizeof(StopEvent);
    return !persistentStore.write_data(pos, (const uint8_t *)&ev, sizeof(ev));
  }

  /**
   * Write the slot first and the header second, so a power loss can never
   * leave the header pointing at an unfinished slot.
   */
  bool append_event(StopEvent ev) {
    if (!journal_ready) return false;

    ev.duration_s = clamp_duration(ev.duration_s);
    ev.crc8 = event_crc(ev);

    const uint8_t index = header.head % JOURNAL_SLOTS;
    if (!persistentStore.access_start()) return false;

    if (!write_event_slot(index, ev)) {
      // Do not leave a half-written slot that a scan could accept
      uint8_t invalid = 0;
      int cpos = PRINT_JOURNAL_SLOTS_ADDR + index * sizeof(StopEvent) + offsetof(StopEvent, crc8);
      persistentStore.write_data(cpos, invalid);
      persistentStore.access_finish();
      return false;
    }

    const JournalHeader old_header = header;
    header.head = (index + 1) % JOURNAL_SLOTS;
    header.count = _MIN(uint16_t(header.count + 1), uint16_t(JOURNAL_SLOTS));
    header.crc16 = header_crc(header);

    int pos = PRINT_JOURNAL_HEADER_ADDR;
    const bool err = persistentStore.write_data(pos, (const uint8_t *)&header, sizeof(header));

    if (err) {
      // Never let the repair scan resurrect a slot the header does not cover
      uint8_t invalid = 0;
      int cpos = PRINT_JOURNAL_SLOTS_ADDR + index * sizeof(StopEvent) + offsetof(StopEvent, crc8);
      persistentStore.write_data(cpos, invalid);
      header = old_header;
      header.crc16 = header_crc(header);
    }

    persistentStore.access_finish();
    if (err) return false;

    events[index] = ev;
    last_seq = ev.seq;
    return true;
  }

  void scan_slots() {
    uint16_t valid = 0;
    uint32_t max_seq = 0;
    uint8_t max_index = 0;
    bool any = false;

    for (uint8_t i = 0; i < JOURNAL_SLOTS; i++) {
      StopEvent &ev = events[i];
      if (ev.seq != 0 && ev.crc8 == event_crc(ev)) {
        ev.file[12] = '\0';
        if (ev.percent > 100 && ev.percent != 0xFF) ev.percent = 0xFF;
        ev.duration_s = clamp_duration(ev.duration_s);
        valid++;
        if (!any || ev.seq > max_seq) { max_seq = ev.seq; max_index = i; any = true; }
      }
      else
        ev.seq = 0;
    }

    if (!any) {
      header.head = 0;
      header.count = 0;
    }
    else if (header.crc16 != header_crc(header) || header.head >= JOURNAL_SLOTS || header.count != valid) {
      header.head = (max_index + 1) % JOURNAL_SLOTS;
      header.count = valid;
    }

    header.crc16 = header_crc(header);
    last_seq = any ? max_seq : 0;
  }

  void read_journal() {
    if (!persistentStore.access_start()) return;
    int pos = PRINT_JOURNAL_HEADER_ADDR;
    persistentStore.read_data(pos, (uint8_t *)&header, sizeof(header));
    pos = PRINT_JOURNAL_SLOTS_ADDR;
    persistentStore.read_data(pos, (uint8_t *)events, sizeof(events));
    pos = PRINT_JOURNAL_ACTIVE_ADDR;
    persistentStore.read_data(pos, (uint8_t *)active_slots, sizeof(active_slots));
    persistentStore.access_finish();
  }

  void consume_emergency_record() {
    if (emergency_record.magic != EMERGENCY_RECORD_MAGIC) return;

    if (emergency_record.crc8 == emergency_crc(emergency_record) && is_known_cause(emergency_record.cause)) {
      emergency_cause_ram = emergency_record.cause;
      emergency_source_ram = emergency_record.source;
      emergency_record_valid = true;
    }
    else
      emergency_detail_lost = true;

    emergency_record.magic = 0;
  }

  const StopEvent& newest_event() {
    const StopEvent *best = nullptr;
    for (uint8_t i = 0; i < JOURNAL_SLOTS; i++)
      if (events[i].seq && (!best || events[i].seq > best->seq)) best = &events[i];
    return best ? *best : no_event;
  }

  /**
   * A valid but still open slot means the previous session was interrupted.
   * Do not duplicate a terminal event already stored just before the reset.
   */
  void synthesize_boot_event() {
    if (!load_active_session() || !session_active) return;

    const StopEvent &last = newest_event();
    if (session.elapsed_s > 0 && last.seq && is_terminal_cause(last.cause) && last.duration_s >= session.elapsed_s)
      { close_active_session(); return; }

    StopEvent ev;
    ev.seq = next_seq();
    ev.duration_s = clamp_duration(session.elapsed_s);
    ev.percent = session.percent;
    ev.stage = session.stage;
    ev.flags = FLAG_ESTIMATED_DURATION;
    copy_filename(ev.file, session.file);

    if (emergency_record_valid) {
      ev.cause = emergency_cause_ram;
      ev.source = emergency_source_ram;
      ev.flags |= FLAG_EMERGENCY | FLAG_UNRECORDED_EMERGENCY;
    }
    else {
      ev.cause = reset_cause(boot_reset);
      ev.source = SOURCE_FIRMWARE;
      if (emergency_detail_lost) ev.flags |= FLAG_EMERGENCY_DETAIL_LOST;
    }

    append_event(ev);
    close_active_session();
  }

} // namespace

void PrintSession::set_boot_reset(const uint8_t mcu) { boot_reset = mcu; }

void PrintSession::init() {
  no_event.cause = CAUSE_UNKNOWN;
  no_event.percent = 0xFF;

  read_journal();

  const bool layout_ok = header.magic == JOURNAL_MAGIC
                      && header.version == JOURNAL_VERSION
                      && header.slots == JOURNAL_SLOTS;

  if (layout_ok)
    scan_slots();
  else {
    header.magic = JOURNAL_MAGIC;
    header.version = JOURNAL_VERSION;
    header.slots = JOURNAL_SLOTS;
    header.head = 0;
    header.count = 0;
    header.crc16 = header_crc(header);
    for (uint8_t i = 0; i < JOURNAL_SLOTS; i++) events[i].seq = 0;
    last_seq = 0;
  }

  consume_emergency_record();

  journal_ready = true;

  synthesize_boot_event();
}

void PrintSession::task() {
  if (!journal_ready) return;

  if (modal_pending) {
    if (marlin_state == MF_KILLED) { modal_pending = false; return; }
    #if HAS_RESUME_CONTINUE
      extern bool wait_for_user;
      if (wait_for_user) return;
    #endif
    modal_pending = false;
    #if HAS_DISPLAY && ENABLED(FB_G6_PRINT_JOURNAL_MODAL)
      ui.show_stop_reason();
    #endif
  }

  if (session_active && (printingIsPaused() || (printingIsActive() && planner.moves_free() == 0)))
    checkpoint(false);
}

void PrintSession::note_source(const StopSource src) { pending_source = src; }

StopSource PrintSession::take_source() {
  const StopSource src = StopSource(pending_source);
  pending_source = SOURCE_UNKNOWN;
  return src;
}

StopCause PrintSession::cause_for_source(const StopSource src) {
  switch (src) {
    case SOURCE_LOCAL_UI: return CAUSE_ABORT_LOCAL_UI;
    case SOURCE_NETWORK:  return CAUSE_ABORT_NETWORK;
    case SOURCE_HOST:     return CAUSE_ABORT_HOST;
    case SOURCE_MEDIA:    return CAUSE_MEDIA_ERROR;
    default:              return CAUSE_UNKNOWN;
  }
}

void PrintSession::note_emergency(const StopCause cause) {
  if (emergency_cause_ram == 0 || (emergency_cause_ram == CAUSE_EMERGENCY_OTHER && cause != CAUSE_EMERGENCY_OTHER))
    emergency_cause_ram = cause;
}

StopCause PrintSession::emergency_cause() {
  return emergency_cause_ram ? StopCause(emergency_cause_ram) : CAUSE_EMERGENCY_OTHER;
}

void PrintSession::commit_emergency() {
  const StopCause cause = emergency_cause();
  const StopSource src = pending_source != SOURCE_UNKNOWN ? take_source() : SOURCE_FIRMWARE;

  emergency_cause_ram = cause;
  emergency_source_ram = src;

  // Record first: it survives a software reset even if the EEPROM write fails
  emergency_record.magic = EMERGENCY_RECORD_MAGIC;
  emergency_record.cause = cause;
  emergency_record.source = src;
  emergency_record.crc8 = emergency_crc(emergency_record);

  if (journal_ready && session_active && !emergency_committed) {
    emergency_committed = true;

    StopEvent ev;
    ev.seq = next_seq();
    ev.duration_s = clamp_duration(print_job_timer.duration());
    ev.percent = current_percent();
    ev.cause = cause;
    ev.source = src;
    ev.stage = current_stage();
    ev.flags = FLAG_EMERGENCY;
    copy_filename(ev.file, session.file);

    if (append_event(ev)) close_active_session();

    session_active = false;
  }

  pending_source = SOURCE_UNKNOWN;
}

void PrintSession::start_session() {
  if (!journal_ready) return;

  if (session_active) { checkpoint(true); return; }

  const uint32_t base_seq = _MAX(_MAX(active_slots[0].seq, active_slots[1].seq), session.seq);

  memset(&session, 0, sizeof(session));
  session.magic = ACTIVE_SESSION_MAGIC;
  session.seq = base_seq >= 0xFFFFFFFFUL ? 1UL : base_seq + 1;
  session.percent = 0;
  session.stage = STAGE_PRINTING;
  session.active = 1;
  copy_filename(session.file, card.filename);

  if (write_active()) {
    session_active = true;
    last_checkpoint_ms = millis();
  }

  sd_read_errors = 0;
  modal_pending = false;
  emergency_committed = false;
}

void PrintSession::checkpoint(const bool force) {
  if (!journal_ready || !session_active) return;

  #if FB_G6_PRINT_JOURNAL_CHECKPOINT_S > 0
    if (!force && !ELAPSED(millis(), last_checkpoint_ms + SEC_TO_MS((millis_t)FB_G6_PRINT_JOURNAL_CHECKPOINT_S))) return;
  #else
    if (!force) return;
  #endif

  session.elapsed_s = clamp_duration(print_job_timer.duration());
  session.percent = current_percent();
  session.stage = current_stage();
  session.seq++;
  write_active();
  last_checkpoint_ms = millis();
}

void PrintSession::end_session(const StopCause cause, const StopSource source) {
  if (!journal_ready || !session_active) return;

  StopEvent ev;
  ev.seq = next_seq();
  ev.duration_s = clamp_duration(print_job_timer.duration());
  ev.percent = current_percent();
  ev.cause = cause;
  ev.source = source;
  ev.stage = cause == CAUSE_COMPLETED ? STAGE_FINISHING : current_stage();
  ev.flags = FLAG_NONE;
  copy_filename(ev.file, card.filename[0] ? card.filename : session.file);

  append_event(ev);

  close_active_session();
  session_active = false;

  modal_pending = cause == CAUSE_ABORT_LOCAL_UI
               || cause == CAUSE_ABORT_NETWORK
               || cause == CAUSE_ABORT_HOST
               || cause == CAUSE_MEDIA_ERROR
               || cause == CAUSE_INTERNAL_STOP;
}

bool PrintSession::note_read_error() {
  if (sd_read_errors < 0xFF) sd_read_errors++;
  return sd_read_errors >= FB_G6_SD_READ_ERROR_LIMIT;
}

void PrintSession::clear_read_errors() { sd_read_errors = 0; }

const StopEvent& PrintSession::last_event() { return newest_event(); }

const StopEvent* PrintSession::event_at(const uint8_t index) {
  const StopEvent *found = nullptr;
  uint32_t previous = 0;
  bool has_previous = false;

  for (uint8_t n = 0; n <= index; n++) {
    const StopEvent *best = nullptr;
    for (uint8_t i = 0; i < JOURNAL_SLOTS; i++) {
      const StopEvent &ev = events[i];
      if (!ev.seq) continue;
      if (has_previous && ev.seq >= previous) continue;
      if (!best || ev.seq > best->seq) best = &ev;
    }
    if (!best) return nullptr;
    found = best;
    previous = best->seq;
    has_previous = true;
  }
  return found;
}

uint8_t PrintSession::count() { return header.count; }

FSTR_P PrintSession::cause_text(const StopCause cause) {
  switch (cause) {
    case CAUSE_COMPLETED:         return GET_TEXT_F(MSG_STOP_CAUSE_COMPLETED);
    case CAUSE_ABORT_LOCAL_UI:
    case CAUSE_ABORT_NETWORK:
    case CAUSE_ABORT_HOST:        return GET_TEXT_F(MSG_STOP_CAUSE_ABORTED);
    case CAUSE_MEDIA_ERROR:       return GET_TEXT_F(MSG_STOP_CAUSE_MEDIA);
    case CAUSE_RESET_WATCHDOG:    return GET_TEXT_F(MSG_STOP_CAUSE_RESET_WATCHDOG);
    case CAUSE_RESET_POWER:       return GET_TEXT_F(MSG_STOP_CAUSE_RESET_POWER);
    case CAUSE_RESET_SOFTWARE:    return GET_TEXT_F(MSG_STOP_CAUSE_RESET_SOFTWARE);
    case CAUSE_EMERGENCY_THERMAL: return GET_TEXT_F(MSG_STOP_CAUSE_EMERGENCY_THERMAL);
    case CAUSE_EMERGENCY_M112:    return GET_TEXT_F(MSG_STOP_CAUSE_EMERGENCY_M112);
    case CAUSE_EMERGENCY_OTHER:   return GET_TEXT_F(MSG_STOP_CAUSE_EMERGENCY);
    case CAUSE_INTERNAL_STOP:     return GET_TEXT_F(MSG_STOP_CAUSE_INTERNAL);
    default:                      return GET_TEXT_F(MSG_STOP_CAUSE_UNKNOWN);
  }
}

FSTR_P PrintSession::source_text(const StopSource source) {
  switch (source) {
    case SOURCE_LOCAL_UI: return GET_TEXT_F(MSG_STOP_SOURCE_LOCAL);
    case SOURCE_NETWORK:  return GET_TEXT_F(MSG_STOP_SOURCE_NETWORK);
    case SOURCE_HOST:     return GET_TEXT_F(MSG_STOP_SOURCE_HOST);
    case SOURCE_MEDIA:    return GET_TEXT_F(MSG_STOP_SOURCE_MEDIA);
    case SOURCE_FIRMWARE: return GET_TEXT_F(MSG_STOP_SOURCE_FIRMWARE);
    default:              return GET_TEXT_F(MSG_STOP_SOURCE_UNKNOWN);
  }
}

void PrintSession::format_duration(char * const buffer, const uint32_t seconds, const bool estimated) {
  const uint32_t duration = clamp_duration(seconds);
  char *p = buffer;
  if (estimated) *p++ = '~';
  sprintf_P(p, PSTR("%02u:%02u:%02u"),
    unsigned(duration / 3600), unsigned((duration / 60) % 60), unsigned(duration % 60)
  );
}

#endif // FB_G6_PRINT_JOURNAL
