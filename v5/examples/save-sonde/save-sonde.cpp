// save-sonde: a one-off probe of level 3's files (module/build_m3_card.h), for base 44 (release 0.26).
// It changes no place of the firmware and names no address of it. It is not meant to be published.
//
// What it does, once: after the first frames, with the disk mode shut, it runs one test a frame and says each
// result in one line on the unit's log, and writes all results so far into results.log in its own folder,
// user/pf/save-sonde/, after each test. The computer sees that folder in the disk mode.
//   A  reads of names that try to reach outside the folder (read only; nothing outside the folder is written);
//   B  writes of 256 KiB to 2.125 MiB of 0xAA, each read back and counted, then removed;
//   C  names with a slash or a backslash, and a list of the folder.
// It runs again at a start only when results.log is gone or does not end with "END".
//
// The frame function alone touches files. There is no input function. Nothing is divided.

#include "build_m3_card.h"

typedef unsigned int u32;
typedef unsigned char u8;

// R1's remove (module/build_r6e_card.h), offered by every base since R1 to a card with folders.
extern "C" int op1f_remove(const Op1fCard* card, unsigned folder, const char* name) OP1F_NAMED("op1f_remove");

extern "C" const Op1fCard op1f_card_save_sonde OP1F_NAMED("op1f_card_save_sonde");

enum { FOLDER = 0, FRAME_LEVEL_3 = 36, START_FRAME = 180, TEXT_ROOM = 6144, LINE_ROOM = 160, NAMES_ROOM = 64 * OP1F_FILE_NAME };
enum { MIB = 1 << 20, BIG_ROOM = MIB + 4 };

static const char LOG_FILE[] = "op1f/save-sonde.cpp";      // the log shows "save-sonde"
static const char RESULTS[] = "results.log";
static const Op1fCard* const CARD = &op1f_card_save_sonde;

// One megabyte and a little: the room of every read and of the writes up to 1 MiB + 1. A write asked for more names
// this room too: the base refuses any write over a megabyte before it reads a byte of the data.
static u8 big[BIG_ROOM] __attribute__((aligned(32)));

static struct {
    u32 step, done, skipped;
    u32 last_ms;                 // op1f_ms at the last test's frame
    char text[TEXT_ROOM];
    u32 at;
    char line[LINE_ROOM];
    char names[NAMES_ROOM];
} s;

// ---------------------------------------------------------------- texts

static u32 put(char* out, u32 at, u32 room, const char* text) {
    while (*text && at + 1 < room) out[at++] = *text++;
    out[at] = 0;
    return at;
}

static u32 put_u(char* out, u32 at, u32 room, u32 n) {
    char digits[12];
    u32 k = 0;
    do {
        u32 tens = 0;
        while (n >= 10u) { n -= 10u; ++tens; }                 // by subtraction: nothing is divided
        digits[k++] = (char)('0' + n);
        n = tens;
    } while (n != 0u && k < sizeof digits);
    while (k > 0 && at + 1 < room) out[at++] = digits[--k];
    out[at] = 0;
    return at;
}

static u32 put_i(char* out, u32 at, u32 room, int n) {
    if (n < 0) { at = put(out, at, room, "-"); return put_u(out, at, room, (u32)(-n)); }
    return put_u(out, at, room, (u32)n);
}

static const char* said(int answer) {
    switch (answer) {
    case OP1F_NO_FILE: return "NO_FILE";
    case OP1F_TOO_LARGE: return "TOO_LARGE";
    case OP1F_NOT_NOW: return "NOT_NOW";
    case OP1F_NO_FOLDER: return "NO_FOLDER";
    case OP1F_BAD_NAME: return "BAD_NAME";
    case OP1F_FAILED: return "FAILED";
    default: return answer >= 0 ? "ok" : "?";
    }
}

// The line in s.line: "<id>: <what> -> <answer> (<name>)" and what `more` adds.
static u32 begin(const char* id, const char* what) {
    u32 at = put(s.line, 0, LINE_ROOM, id);
    at = put(s.line, at, LINE_ROOM, ": ");
    return put(s.line, at, LINE_ROOM, what);
}

static u32 answer(u32 at, const char* label, int a) {
    at = put(s.line, at, LINE_ROOM, label);
    at = put_i(s.line, at, LINE_ROOM, a);
    at = put(s.line, at, LINE_ROOM, " ");
    return put(s.line, at, LINE_ROOM, said(a));
}

// The line said on the log and kept for results.log.
static void say() {
    op1f_log(3, LOG_FILE, (int)s.step, "%s", s.line);
    s.at = put(s.text, s.at, TEXT_ROOM, s.line);
    s.at = put(s.text, s.at, TEXT_ROOM, "\n");
}

static void save_results() {
    int w = op1f_write(CARD, FOLDER, RESULTS, s.text, s.at);
    if (w != (int)s.at) op1f_log(1, LOG_FILE, 900, "results.log NOT written: %d %s", w, said(w));
}

// ---------------------------------------------------------------- the memory

static void fill(u32 bytes, u8 value) {
    volatile u8* p = big;                                       // volatile: no call of memset is made of the loop
    for (u32 i = 0; i < bytes; ++i) p[i] = value;
}

static u32 count_aa(u32 bytes) {
    const volatile u8* p = big;
    u32 n = 0;
    for (u32 i = 0; i < bytes; ++i) n += p[i] == 0xAAu ? 1u : 0u;
    return n;
}

// ---------------------------------------------------------------- the tests

struct Read { const char* id; const char* name; };
static const Read READS[] = {
    { "A1", "synth_slots.json" },
    { "A2", "../synth_slots.json" },
    { "A3", "../../user/synth_slots.json" },
    { "A4", "/user/synth_slots.json" },
    { "A5", "user/synth_slots.json" },
    { "A6", "tape01/tape.json" },
    { "A8", "..\\..\\synth_slots.json" },                       // a backslash: no slash, so the base takes the name
    { "A9", "..\\..\\..\\user\\synth_slots.json" },
};
enum { READS_COUNT = sizeof READS / sizeof READS[0] };

struct Write { const char* id; const char* name; u32 bytes; };
static const Write WRITES[] = {
    { "B1", "b1.bin", 262144u },
    { "B2", "b2.bin", 524288u },
    { "B3", "b3.bin", 1048576u },
    { "B4", "b4.bin", 1048577u },
    { "B5", "b5.bin", 1572864u },
    { "B6", "b6.bin", 2097152u },
    { "B7", "b7.bin", 2228224u },
};
enum { WRITES_COUNT = sizeof WRITES / sizeof WRITES[0] };

static const char* const C_NAMES[] = { "sub/t.txt", "deep/nested/dir/t.txt", "sub\\t.txt" };
static const char* const C_IDS[] = { "C1", "C2", "C5" };
enum { C_COUNT = 3 };

static void test_read(const Read& r) {
    int got = op1f_read(CARD, FOLDER, r.name, big, BIG_ROOM);  // read only: whatever it reaches is not changed
    u32 at = begin(r.id, "read '");
    at = put(s.line, at, LINE_ROOM, r.name);
    at = put(s.line, at, LINE_ROOM, "'");
    answer(at, " -> ", got);
    say();
}

static void test_path() {
    char path[96];
    int n = op1f_path(CARD, FOLDER, 0, path, sizeof path);
    u32 at = answer(begin("A0", "op1f_path(folder 0)"), " -> ", n);
    if (n > 0) { at = put(s.line, at, LINE_ROOM, " '"); at = put(s.line, at, LINE_ROOM, path); put(s.line, at, LINE_ROOM, "'"); }
    say();
    n = op1f_path(CARD, FOLDER, "synth_slots.json", path, sizeof path);
    at = answer(begin("A7", "op1f_path('synth_slots.json'); no root variant exists"), " -> ", n);
    if (n > 0) { at = put(s.line, at, LINE_ROOM, " '"); at = put(s.line, at, LINE_ROOM, path); put(s.line, at, LINE_ROOM, "'"); }
    say();
    n = op1f_read(CARD, 1, "synth_slots.json", big, BIG_ROOM);
    answer(begin("A10", "read folder 1 (not the card's)"), " -> ", n);
    say();
}

// B: a write, then a read of the same name into the cleared room, the bytes of 0xAA counted.
static void test_write(const Write& w) {
    if (w.bytes <= (u32)BIG_ROOM) fill(w.bytes, 0xAAu);
    u32 before = op1f_ms();
    int put_answer = op1f_write(CARD, FOLDER, w.name, big, w.bytes);
    fill(BIG_ROOM, 0);
    int got = op1f_read(CARD, FOLDER, w.name, big, BIG_ROOM);
    u32 at = begin(w.id, "write ");
    at = put_u(s.line, at, LINE_ROOM, w.bytes);
    at = answer(at, " -> ", put_answer);
    at = answer(at, "; read back -> ", got);
    if (got > 0) { at = put(s.line, at, LINE_ROOM, "; 0xAA bytes "); at = put_u(s.line, at, LINE_ROOM, count_aa((u32)got)); }
    at = put(s.line, at, LINE_ROOM, "; ms ");
    put_u(s.line, at, LINE_ROOM, op1f_ms() - before);           // 0 within a frame: the clock moves once a frame; see the gap line
    say();
}

static void test_bad_write(u32 k) {
    int w = op1f_write(CARD, FOLDER, C_NAMES[k], "hello", 5);
    u32 at = begin(C_IDS[k], "write '");
    at = put(s.line, at, LINE_ROOM, C_NAMES[k]);
    at = put(s.line, at, LINE_ROOM, "'");
    answer(at, " -> ", w);
    say();
}

static void test_list(const char* id) {
    int n = op1f_list(CARD, FOLDER, s.names, NAMES_ROOM);
    u32 at = answer(begin(id, "list folder 0"), " -> ", n);
    u32 from = 0;
    for (int i = 0; i < n && i < 12; ++i) {
        at = put(s.line, at, LINE_ROOM, i ? ", " : ": ");
        at = put(s.line, at, LINE_ROOM, s.names + from);
        while (s.names[from]) ++from;
        ++from;
    }
    say();
}

static void test_list_other() {
    int n = op1f_list(CARD, 1, s.names, NAMES_ROOM);
    answer(begin("C4b", "list folder 1 (not the card's)"), " -> ", n);
    say();
}

static void clean() {
    u32 at = begin("Z1", "removed");
    for (u32 k = 0; k < (u32)WRITES_COUNT; ++k) {
        int r = op1f_remove(CARD, FOLDER, WRITES[k].name);
        at = put(s.line, at, LINE_ROOM, " ");
        at = put(s.line, at, LINE_ROOM, WRITES[k].id);
        at = put(s.line, at, LINE_ROOM, "=");
        at = put_i(s.line, at, LINE_ROOM, r);
    }
    for (u32 k = 0; k < (u32)C_COUNT; ++k) {
        int r = op1f_remove(CARD, FOLDER, C_NAMES[k]);           // a name with a slash answers BAD_NAME: nothing to remove
        at = put(s.line, at, LINE_ROOM, " ");
        at = put(s.line, at, LINE_ROOM, C_IDS[k]);
        at = put(s.line, at, LINE_ROOM, "=");
        at = put_i(s.line, at, LINE_ROOM, r);
    }
    say();
}

// The order: A0/A7/A10, A1..A9, B1..B7, C1, C2, C5, C4, C4b, the clean, the list again, the end.
enum { T_PATH = 0, T_READS = 1, T_WRITES = T_READS + READS_COUNT, T_BAD = T_WRITES + WRITES_COUNT, T_LIST = T_BAD + C_COUNT,
       T_LIST_OTHER, T_CLEAN, T_LIST_AFTER, T_END };

static void run(u32 t) {
    if (t == T_PATH) test_path();
    else if (t < (u32)T_WRITES) test_read(READS[t - T_READS]);
    else if (t < (u32)T_BAD) test_write(WRITES[t - T_WRITES]);
    else if (t < (u32)T_LIST) test_bad_write(t - T_BAD);
    else if (t == T_LIST) test_list("C4");
    else if (t == T_LIST_OTHER) test_list_other();
    else if (t == T_CLEAN) clean();
    else if (t == T_LIST_AFTER) test_list("Z2");
}

// A results.log that ends with "END" means the probe ran to its end at an earlier start.
static int ran_before() {
    int n = op1f_read(CARD, FOLDER, RESULTS, s.text, TEXT_ROOM - 1);
    if (n < 4) return 0;
    return s.text[n - 4] == 'E' && s.text[n - 3] == 'N' && s.text[n - 2] == 'D' && s.text[n - 1] == '\n';
}

// Once a frame, on the display thread.
extern "C" void save_sonde_frame(const Op1fFrame* now) OP1F_NAMED("save_sonde_frame");
extern "C" void save_sonde_frame(const Op1fFrame* now) {
    if (now->bytes < (u32)FRAME_LEVEL_3 || s.done || s.skipped) return;
    if (now->frame < (u32)START_FRAME || now->disk) return;    // the start settled, and the computer not owning the folders
    if (s.step == 0 && s.last_ms == 0) {
        if (ran_before()) {
            s.skipped = 1;
            op1f_log(3, LOG_FILE, 1, "save-sonde: results.log ends with END: not run again (delete it to run the probe again)");
            return;
        }
        s.at = put(s.text, 0, TEXT_ROOM, "save-sonde 1, base 44 probe of level 3's files\n");
        s.last_ms = op1f_ms() | 1u;
    } else {
        u32 gap = op1f_ms() - s.last_ms;                        // the time the last test held the frame, about
        s.last_ms = op1f_ms() | 1u;
        u32 at = put(s.line, 0, LINE_ROOM, "  gap ");
        at = put_u(s.line, at, LINE_ROOM, gap);
        put(s.line, at, LINE_ROOM, " ms");
        s.at = put(s.text, s.at, TEXT_ROOM, s.line);           // in the file only: the log keeps one line a test
        s.at = put(s.text, s.at, TEXT_ROOM, "\n");
    }
    if (s.step >= (u32)T_END) {
        s.at = put(s.text, s.at, TEXT_ROOM, "END\n");
        save_results();
        op1f_log(3, LOG_FILE, 999, "save-sonde: END, results in user/pf/save-sonde/results.log");
        s.done = 1;
        return;
    }
    run(s.step);
    save_results();
    ++s.step;
}
