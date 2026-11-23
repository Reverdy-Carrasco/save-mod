// The pack `save mod`, version 3, for OS 1.7.3+0 of the OP-1 Field: the mixer's pages 2, 3 and 4 and the tape's speed
// kept with each tape, through level 7 of the base (docs/level-7.md). It changes no place of the firmware.
//
// What it keeps, as one member of the tape's file, `save_mod`, its members in the order of their names:
//
//   {"eq":[a,b,c],"fx":[a,b,c,d,e,f,g,h],"fx_type":"delay","master":[l,r,drive,release],"tape_speed":n,"version":2}
//
//   eq          the mixer's page 2, the master EQ: three values
//   fx          the mixer's page 3: the eight values of the master effect
//   fx_type     the mixer's page 3: which master effect is on, by the name the unit's own settings file gives it
//   master      the mixer's page 4, the master compressor: left, right, drive, release
//   tape_speed  the tape's speed in semitones times 1024, from -24 x 1024 to +12 x 1024
//   version     this form's version, 2. Version 1 (save mod 1) had no fx_type: its values are still taken, and the
//               effect that is on stays on. A value of another version is taken as none
//
// Each value of eq, fx and master is the 16-bit signed number the unit's own settings file holds for it: the unit keeps
// each in the upper half of a 32-bit word, and its own save writes that half, its own load puts it back there.
//
// What the owner meets:
//   a tape loaded, switched to, made or copied: the tape's values, when it has save mod's; when it has none (a tape from
//     before, or a new tape), the values the unit had at the start, which are those of its own settings file;
//   a value changed on one of the three pages or the tape's speed changed: the tape's values change with it, and they
//     are written into the tape's file when the unit next writes the tape (the unit decides when; the pack cannot).
//
// Where it meets the firmware (OS 1.7.3+0 only):
//   0x849a8998  the master EQ's three words           read and written by the unit's own save and load of its settings
//   0x8c0bacb4  the function that hands the EQ's three words to the sound (no argument, nothing back): the unit
//               calls it after each change of the EQ, from its page and from its own load; the pack calls it after
//               it has written the words
//   0x11903be0  the master effect's eight words       the same; the audio code reads them
//   0x11b01e8c  the master compressor's four words    the same; the audio code reads them
//   0x84c62228  the tape's speed, a 16-bit word, semitones x 1024
//   0x8d50ff34  the function that changes the tape's speed by a step (which, step): called with a step of 0 after
//               the word is written, so that the unit takes the new speed as it takes one from its knob
//   0x11902e24  which master effect is on: its type, a 32-bit word the unit's own load and pick set
//   0x8490e288  the firmware's eleven effects: pairs of words, the name (a text) and the type
//   0x8f008e28  the function that builds an effect of a type (which, type), which 1 for the master: the unit's own
//               load of its settings file calls it with the type it read, then the type word holds that type; the
//               pack does the same. (The unit's pick from its list takes another number, the row's id in the unit's
//               list of effects, which is not the type: version 2 of the pack called the pick with the type, and the
//               unit put another effect on.) The base builds its packs' effects inside that function, and ends a
//               pack's effect riding the master when the type word no longer holds the effect it rides
//   0x840afe60  the knobs' ranges by type: the first four of the eight values are held to the range of the effect
//               that is on before they are written (a value out of its range makes the unit say "Master fx param out
//               of range" at every frame its page reads it)
// The values are read once a frame and written only after a tape load, on the display thread, where the unit's own
// pages change them. A 32-bit word is written whole, so the audio code never reads half a value.
//
// What has been tried: version 3 runs on a unit (OS 1.7.3+0, open-field 0.12): each tape brings back its values.

#include "build_r4b_card.h"

typedef unsigned int u32;
typedef unsigned short u16;

extern "C" const Op1fCard op1f_card_save_mod OP1F_NAMED("op1f_card_save_mod");

enum { FRAME_LEVEL_4 = 40, EQ_COUNT = 3, FX_COUNT = 8, MASTER_COUNT = 4, VALUES = EQ_COUNT + FX_COUNT + MASTER_COUNT };
enum { KEPT_VERSION = 2, KEPT_ROOM = 320, LINES_MOST = 24, EFFECTS = 11, NAME_MOST = 15 };
enum { SPEED_LEAST = -24 * 1024, SPEED_MOST = 12 * 1024 };

static volatile int* const EQ_WORDS = (volatile int*)0x849a8998u;
static volatile int* const FX_WORDS = (volatile int*)0x11903be0u;
static volatile int* const MASTER_WORDS = (volatile int*)0x11b01e8cu;
static volatile short* const TAPE_SPEED = (volatile short*)0x84c62228u;
typedef void (*SpeedStepFn)(int which, int step);
#define STOCK_SPEED_STEP ((SpeedStepFn)0x8d50ff34u)
typedef void (*EqApplyFn)();
#define STOCK_EQ_APPLY ((EqApplyFn)0x8c0bacb4u)
static volatile int* const FX_TYPE = (volatile int*)0x11902e24u;
struct EffectName { const char* name; int id; };
static const EffectName* const EFFECT_NAMES = (const EffectName*)0x8490e288u;
typedef void (*EffectBuildFn)(int which, int type);
#define STOCK_EFFECT_BUILD ((EffectBuildFn)0x8f008e28u)
enum { BUILD_MASTER = 1 };
// The knobs' ranges by type, filled by the unit at its start: 48 bytes a type, 6 a knob, the least (signed) and the
// most (unsigned) of the knob's word's upper half. The base reads the first four, its effects' knobs, the same way.
static const unsigned char* const KNOB_ROWS = (const unsigned char*)0x840afe60u;
enum { ROW_BYTES = 48, KNOB_BYTES = 6, KNOBS = 4 };

static const char LOG_FILE[] = "op1f/save_mod.cpp";       // the log shows "save_mod"
static const char KEPT[] = "save_mod";

// One set of what the pack keeps: the fifteen 16-bit values of the three pages, the master effect and the speed.
struct Kept {
    int v[VALUES];                                        // eq, then fx, then master
    int speed;
    int effect;                                           // the master effect's number, 0 to EFFECTS - 1; -1 for none kept
};

// Written by save_mod_changed (the display thread, outside the frame function), taken by the frame function.
static volatile u32 told;
static volatile u32 last_load;

static struct {
    u32 started;
    u32 tape;                                             // a tape was told and taken: the base keeps the values with it
    u32 load_waits;                                       // a load told and not taken yet (the base said not now)
    u32 held;                                             // what the base holds for the tape is `last`
    u32 lines;
    Kept start;                                           // the unit's values at the first frame: a tape with none takes them
    Kept last;                                            // what was last read from or set into the tape
    char text[KEPT_ROOM];
} s;

static void say(int level, int line, const char* format, int a = 0, int b = 0) {
    if (s.lines >= (u32)LINES_MOST) return;
    ++s.lines;
    op1f_log(level, LOG_FILE, line, format, a, b);
}

extern "C" void save_mod_changed(unsigned where, unsigned which, unsigned how) OP1F_NAMED("save_mod_changed");
extern "C" void save_mod_changed(unsigned where, unsigned which, unsigned how) {
    if (where != (unsigned)OP1F_KEEP_TAPE || which != 0 || how > 31u) return;
    told |= 1u << how;
    if (how == (unsigned)OP1F_KEPT_LOADED || how == (unsigned)OP1F_KEPT_MADE) last_load = how;
}

// ---------------------------------------------------------------- the unit's values

static int upper(int word) { return word >> 16; }         // the 16-bit value the unit's own file holds for a word

static void read_unit(Kept* k) {
    for (int i = 0; i < EQ_COUNT; ++i) k->v[i] = upper(EQ_WORDS[i]);
    for (int i = 0; i < FX_COUNT; ++i) k->v[EQ_COUNT + i] = upper(FX_WORDS[i]);
    for (int i = 0; i < MASTER_COUNT; ++i) k->v[EQ_COUNT + FX_COUNT + i] = upper(MASTER_WORDS[i]);
    k->speed = *TAPE_SPEED;
    int e = *FX_TYPE;
    k->effect = e >= 0 && e < (int)EFFECTS ? e : -1;
}

// Copied value by value: a copy of the whole struct could become a call of memcpy, which the module does not link.
static void copy(Kept* to, const Kept* from) {
    for (int i = 0; i < VALUES; ++i) to->v[i] = from->v[i];
    to->speed = from->speed;
    to->effect = from->effect;
}

static int same(const Kept* a, const Kept* b) {
    for (int i = 0; i < VALUES; ++i) if (a->v[i] != b->v[i]) return 0;
    return a->speed == b->speed && a->effect == b->effect;
}

// A word set as the unit's own load sets it: the value in the upper half, the lower half 0. Left alone when its upper
// half already holds the value, so that a value that did not change keeps its lower half as the unit left it.
// Answers 1 when the word was written.
static int put_word(volatile int* w, int v) {
    if (upper(*w) == v) return 0;
    *w = (int)((u32)(u16)v << 16);
    return 1;
}

// The name the unit's list gives an effect's number, or 0.
static const char* effect_name(int id) {
    for (int i = 0; i < (int)EFFECTS; ++i) if (EFFECT_NAMES[i].id == id) return EFFECT_NAMES[i].name;
    return 0;
}

// The number of the effect of that name in the unit's list (n characters at name), or -1.
static int effect_id(const char* name, u32 n) {
    for (int i = 0; i < (int)EFFECTS; ++i) {
        const char* e = EFFECT_NAMES[i].name;
        u32 k = 0;
        while (k < n && e[k] && e[k] == name[k]) ++k;
        if (k == n && e[k] == 0) return EFFECT_NAMES[i].id;
    }
    return -1;
}

// The master's effect put on as the unit's own load of its settings file puts it on: built for the type, then the type
// word set. The effect's eight values are written after it, as that load has them.
static void put_effect(int type) {
    STOCK_EFFECT_BUILD(BUILD_MASTER, type);
    *FX_TYPE = type;
}

// A knob's value held to the range of its row for the effect of that type; as it was where the row has no span.
static int held_to_row(int type, int knob, int v) {
    const unsigned char* row = KNOB_ROWS + ROW_BYTES * type + KNOB_BYTES * knob;
    int low = (short)(row[0] | (row[1] << 8));
    int high = (int)(row[2] | (row[3] << 8));
    if (high <= low) return v;
    int x = low < 0 ? v : (int)(u16)v;                    // a row from 0 reads the upper half as unsigned, as the base does
    if (x < low) x = low;
    if (x > high) x = high;
    return (int)(short)(u16)x;
}

static void write_unit(const Kept* k) {
    if (k->effect >= 0 && k->effect != *FX_TYPE) put_effect(k->effect);
    int eq = 0;
    for (int i = 0; i < EQ_COUNT; ++i) eq |= put_word(&EQ_WORDS[i], k->v[i]);
    if (eq) STOCK_EQ_APPLY();                             // the EQ's words reach the sound only through it
    int type = *FX_TYPE;
    for (int i = 0; i < FX_COUNT; ++i) {
        int v = k->v[EQ_COUNT + i];
        if (i < (int)KNOBS && type >= 0 && type < (int)EFFECTS) v = held_to_row(type, i, v);
        put_word(&FX_WORDS[i], v);
    }
    for (int i = 0; i < MASTER_COUNT; ++i) put_word(&MASTER_WORDS[i], k->v[EQ_COUNT + FX_COUNT + i]);
    if (*TAPE_SPEED != k->speed) {
        *TAPE_SPEED = (short)k->speed;
        STOCK_SPEED_STEP(0, 0);                           // the unit takes the speed, held to its range
    }
}

// ---------------------------------------------------------------- the member's text

static const char* skip(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

// A whole number at p, at most six digits. 0 when there is none.
static const char* number(const char* p, int* out) {
    int negative = *p == '-';
    if (negative) ++p;
    if (*p < '0' || *p > '9') return 0;
    int v = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 6) return 0;
        v = v * 10 + (*p - '0');
        ++p;
    }
    if (*p == '.' || *p == 'e' || *p == 'E') return 0;
    *out = negative ? -v : v;
    return p;
}

// A list of exactly `count` whole numbers, each held to 16 bits.
static const char* list(const char* p, int* out, int count) {
    if (*p != '[') return 0;
    p = skip(p + 1);
    for (int i = 0; i < count; ++i) {
        int v;
        p = number(p, &v);
        if (!p) return 0;
        out[i] = v < -32768 ? -32768 : v > 32767 ? 32767 : v;
        p = skip(p);
        if (i + 1 < count) {
            if (*p != ',') return 0;
            p = skip(p + 1);
        }
    }
    return *p == ']' ? p + 1 : 0;
}

static int named(const char* name, u32 n, const char* word) {
    u32 i = 0;
    for (; i < n; ++i) if (word[i] != name[i]) return 0;
    return word[i] == 0;
}

// The values out of the member's text. 1 when it holds eq, fx, master, tape_speed and version, its version is 1 or 2,
// and each member is of its form; 0 otherwise, nothing taken. fx_type is of version 2 and may be missing (an effect
// the unit's list did not name when it was set); a name the unit's list does not hold keeps the effect that is on.
// A member of another name is a fault: neither version writes one.
static int read_kept(const char* t, Kept* k) {
    Kept got;
    got.effect = -1;
    int version = -1;
    u32 have = 0;
    const char* p = skip(t);
    if (*p != '{') return 0;
    p = skip(p + 1);
    for (;;) {
        if (*p != '"') return 0;
        const char* name = ++p;
        while (*p && *p != '"') ++p;
        if (*p != '"') return 0;
        u32 n = (u32)(p - name);
        p = skip(p + 1);
        if (*p != ':') return 0;
        p = skip(p + 1);
        if (named(name, n, "eq")) { p = list(p, &got.v[0], EQ_COUNT); have |= 1; }
        else if (named(name, n, "fx")) { p = list(p, &got.v[EQ_COUNT], FX_COUNT); have |= 2; }
        else if (named(name, n, "fx_type")) {
            if (*p != '"') return 0;
            const char* type = ++p;
            while (*p && *p != '"' && *p != '\\') ++p;
            if (*p != '"') return 0;
            u32 length = (u32)(p - type);
            ++p;
            got.effect = length <= (u32)NAME_MOST ? effect_id(type, length) : -1;   // a name the unit does not know: none
            have |= 32;
        }
        else if (named(name, n, "master")) { p = list(p, &got.v[EQ_COUNT + FX_COUNT], MASTER_COUNT); have |= 4; }
        else if (named(name, n, "tape_speed")) { p = number(p, &got.speed); have |= 8; }
        else if (named(name, n, "version")) { p = number(p, &version); have |= 16; }
        else return 0;
        if (!p) return 0;
        p = skip(p);
        if (*p == ',') { p = skip(p + 1); continue; }
        if (*p != '}' || *skip(p + 1) != 0) return 0;
        break;
    }
    if ((have & 31u) != 31u || version < 1 || version > KEPT_VERSION) return 0;
    if (version == 1 && (have & 32u)) return 0;           // version 1 had no fx_type
    if (got.speed < SPEED_LEAST) got.speed = SPEED_LEAST;   // a file edited by hand: the speed held to its range
    if (got.speed > SPEED_MOST) got.speed = SPEED_MOST;
    copy(k, &got);
    return 1;
}

static u32 put_text(char* out, u32 at, const char* text) {
    while (*text) out[at++] = *text++;
    return at;
}

static u32 put_number(char* out, u32 at, int v) {         // -32768 to 32767
    u32 m = v < 0 ? 0u - (u32)v : (u32)v;
    if (v < 0) out[at++] = '-';
    char digits[6];
    u32 n = 0;
    do {
        u32 q = (m * 52429u) >> 19;                       // m / 10 for m under 43690; then by subtraction
        while (q * 10u > m) --q;
        while ((q + 1u) * 10u <= m) ++q;
        digits[n++] = (char)('0' + (m - q * 10u));
        m = q;
    } while (m);
    while (n) out[at++] = digits[--n];
    return at;
}

static u32 put_list(char* out, u32 at, const int* v, int count) {
    out[at++] = '[';
    for (int i = 0; i < count; ++i) {
        if (i) out[at++] = ',';
        at = put_number(out, at, v[i]);
    }
    out[at++] = ']';
    return at;
}

// 1 for a name of 1 to NAME_MOST characters of a-z, 0-9 and '_': written in the text as it stands.
static int plain(const char* name) {
    u32 n = 0;
    for (; name[n]; ++n) {
        char c = name[n];
        if (n >= (u32)NAME_MOST || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return n > 0;
}

// The member's text, its members in the order of their names, as the base hands it back.
static void text_kept(const Kept* k) {
    char* t = s.text;
    u32 at = put_text(t, 0, "{\"eq\":");
    at = put_list(t, at, &k->v[0], EQ_COUNT);
    at = put_text(t, at, ",\"fx\":");
    at = put_list(t, at, &k->v[EQ_COUNT], FX_COUNT);
    const char* type = k->effect >= 0 ? effect_name(k->effect) : 0;
    if (type && !plain(type)) type = 0;                   // a name that would need escaping: fx_type is left out
    if (type) {
        at = put_text(t, at, ",\"fx_type\":\"");
        at = put_text(t, at, type);
        t[at++] = '"';
    }
    at = put_text(t, at, ",\"master\":");
    at = put_list(t, at, &k->v[EQ_COUNT + FX_COUNT], MASTER_COUNT);
    at = put_text(t, at, ",\"tape_speed\":");
    at = put_number(t, at, k->speed);
    at = put_text(t, at, ",\"version\":2}");
    t[at] = 0;
}

// ---------------------------------------------------------------- the tape

static void tape_loaded() {
    int got = op1f_value_get(&op1f_card_save_mod, OP1F_KEEP_TAPE, 0, KEPT, s.text, sizeof s.text);
    if (got == OP1F_NOT_NOW) return;                      // asked again at the next frame, nothing said
    s.load_waits = 0;
    if (got < 0 && got != OP1F_NO_FILE) {
        s.tape = 0;
        say(2, 1, "save_mod: the base does not keep the values with the tape (%d): nothing is kept", got);
        return;
    }
    s.tape = 1;
    Kept k;
    if (got >= 0 && read_kept(s.text, &k)) {
        if (k.effect < 0) {                               // none kept (version 1): the effect on stays, and its values with it,
            Kept now;                                     // since the tape's may be another effect's
            read_unit(&now);
            k.effect = now.effect;
            for (int i = 0; i < FX_COUNT; ++i) k.v[EQ_COUNT + i] = now.v[EQ_COUNT + i];
        }
        write_unit(&k);
        copy(&s.last, &k);
        s.held = 1;
        say(3, 2, "save_mod: the tape's values taken, tape speed %d", k.speed);
        return;
    }
    if (got >= 0) say(2, 3, "save_mod: the tape's value is not of version 1's or 2's form: the values of the start are taken");
    write_unit(&s.start);                                 // a tape with none: the values of the start, kept from now on
    s.held = 0;
}

static void tape_keep() {
    if (!s.tape || s.load_waits) return;                  // nothing set over a tape whose load is still to be taken
    Kept now;
    read_unit(&now);
    if (s.held && same(&now, &s.last)) return;
    text_kept(&now);
    int answer = op1f_value_set(&op1f_card_save_mod, OP1F_KEEP_TAPE, 0, KEPT, s.text);
    if (answer == OP1F_NOT_NOW) return;                   // set at the next frame, nothing said
    if (answer != 0) {
        s.tape = 0;
        say(2, 4, "save_mod: the base refused the tape's value (%d): nothing is kept until the next tape", answer);
        return;
    }
    copy(&s.last, &now);
    s.held = 1;
}

extern "C" void save_mod_frame(const Op1fFrame* now) OP1F_NAMED("save_mod_frame");
extern "C" void save_mod_frame(const Op1fFrame* now) {
    if (now->bytes < (u32)FRAME_LEVEL_4) return;
    if (!s.started) {                                     // the first frame: the unit's values as its own file set them
        s.started = 1;
        read_unit(&s.start);
        say(3, 5, "save_mod: the values of the start kept, tape speed %d", s.start.speed);
    }
    u32 bits = told;
    told = 0;
    if (bits & ((1u << OP1F_KEPT_LOADED) | (1u << OP1F_KEPT_MADE))) s.load_waits = 1;
    if (s.load_waits) tape_loaded();
    if ((bits & (1u << OP1F_KEPT_SAVED)) && s.tape) say(3, 6, "save_mod: the values written into the tape");
    tape_keep();
}
