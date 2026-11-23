// The pack `save mod`, version 4, for OS 1.7.3+0 of the OP-1 Field: the mixer's pages 2, 3 and 4, the master effect
// (its type, whether it is on, and a pack's effect riding it), the mixer's two memories M1 and M2, and the tape's speed,
// kept with each tape through level 7 of the base (docs/level-7.md). It changes no place of the firmware.
//
// What it keeps, as one member of the tape's file, `save_mod`, its members in the order of their names:
//
//   {"eq":[a,b,c],"fx":[a,b,c,d,e,f,g,h],"fx_active":1,"fx_effect":"ping","fx_type":"delay",
//    "m1":{"has_data":1,"params":[a,b,c,d,e,f,g,h],"subscreen":1},"m2":{...},
//    "master":[l,r,drive,release],"tape_speed":n,"version":3}
//
//   eq          the mixer's page 2, the master EQ: three values
//   fx          the mixer's page 3: the eight values of the master effect
//   fx_active   the master effect switched on (1) or off (0)
//   fx_effect   the pack's effect riding the master, by its name in the base's list; left out when none rides it
//   fx_type     which of the unit's own effects is on the master (the one a pack's effect rides, when one does), by the
//               name the unit's own settings file gives it
//   m1, m2      the mixer's two memories (the keys under the knobs 7 and 8 on the tape's and the mixer's pages): whether
//               it holds anything, its eight values, and the page of the mixer it brings back (0 the levels, 1 the EQ,
//               2 the effect, 3 the compressor)
//   master      the mixer's page 4, the master compressor: left, right, drive, release
//   tape_speed  the tape's speed in semitones times 1024, from -24 x 1024 to +12 x 1024
//   version     this form's version, 3. Forms 1 (save mod 1) and 2 (save mod 2 and 3) are still taken: what they do not
//               hold stays as it is (the effect and its values for form 1, a pack's effect riding the master, M1 and
//               M2), and the switch takes the start's. A value of another version is taken as none
//
// Each value of eq, fx, master and of a memory's params is the 16-bit signed number the unit's own settings file holds
// for it: the unit keeps each in the upper half of a 32-bit word, and its own save writes that half.
//
// What the owner meets:
//   a tape loaded, switched to, made or copied: the tape's values, when it has save mod's; when it has none (a tape from
//     before, or a new tape), the values the unit had at the start, which are those of its own settings file;
//   a value changed on one of the three pages, the effect, its switch, a memory, or the tape's speed: the tape's values
//     change with it, and they are written into the tape's file when the unit next writes the tape.
//
// Where it meets the firmware (OS 1.7.3+0 only):
//   0x849a8998  the master EQ's three words
//   0x8c0bacb4  the function that hands the EQ's three words to the sound (no argument): the unit calls it after each
//               change of the EQ; the pack calls it after it has written the words
//   0x11903be0  the master effect's eight words, which the audio code reads
//   0x11b01e8c  the master compressor's four words, which the audio code reads
//   0x84c62228  the tape's speed, a 16-bit word, semitones x 1024
//   0x8d50ff34  the function that changes the tape's speed by a step (which, step): called with a step of 0 after the
//               word is written, so that the unit takes the new speed as it takes one from its knob
//   0x11902e24  which of the unit's effects is on the master: its type, a 32-bit word
//   0x8490e288  the firmware's eleven effects: pairs of words, the name (a text) and the type
//   0x8f008e28  the function that builds an effect of a type (which, type), which 1 for the master: the unit's own
//               load of its settings file calls it with the type it read, then the type word holds that type; the pack
//               does the same for one of the unit's own effects. The base ends a pack's effect riding the master when
//               the type word no longer holds the effect it rides
//   0x840afe60  the knobs' ranges by type: the first four of the eight values are held to the range of the effect that
//               is on before they are written (a value out of its range makes the unit say "Master fx param out of
//               range" at every frame its page reads it)
//   0x8d50424c  whether the master effect is switched on (no argument; 1 or 0): the firmware's own answer, under the
//               base's, which tells the unit's file "off" while a pack's effect rides the master
//   0x8d5042c0  the function that switches the master effect on or off (on)
//   0x8c0ba620  the function that answers the mixer's controller (no argument)
//   +0x28       the word of the controller's table of functions that puts an effect of the unit's list on the master
//               (the controller, the row's id in the list). The base stands in that word (op1f_fx_master_pick): a
//               pack's effect is put on through it, by its id in the base's list (op1f_rack_list), which is that
//               row's id. The word is read as the unit holds it, so the pack names no function of the base's own
//   0x849a8a10  the mixer's two memories, M1 then M2, 40 bytes each: the page (a 32-bit word, 0 to 3), at +4 a byte
//               saying it holds something, at +8 the eight 32-bit words of its values (their upper halves are what
//               the unit's own file holds), as the unit's own save and load of its settings read and write them
// The values are read once a frame and written only after a tape load, on the display thread, where the unit's own
// pages change them. A 32-bit word is written whole, so the audio code never reads half a value.
//
// What has been tried: version 3 runs on a unit (OS 1.7.3+0, open-field 0.12). Version 4 is built and its member's
// text tested on a computer only.

#include "build_r6e_card.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

extern "C" const Op1fCard op1f_card_save_mod OP1F_NAMED("op1f_card_save_mod");

enum { FRAME_LEVEL_4 = 40, EQ_COUNT = 3, FX_COUNT = 8, MASTER_COUNT = 4, VALUES = EQ_COUNT + FX_COUNT + MASTER_COUNT };
enum { KEPT_VERSION = 3, KEPT_ROOM = 640, LINES_MOST = 24, EFFECTS = 11, NAME_MOST = 15, NAME_ROOM = 16 };
enum { SPEED_LEAST = -24 * 1024, SPEED_MOST = 12 * 1024 };
enum { MEMORIES = 2, MEMORY_VALUES = 8, MEMORY_BYTES = 40, SUBSCREEN_MOST = 3, RACK_ID_FIRST = 15, RETYPE_FRAMES = 4 };

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
typedef unsigned (*MasterOnFn)();
#define STOCK_MASTER_ON ((MasterOnFn)0x8d50424cu)
typedef void (*SetMasterOnFn)(unsigned on);
#define STOCK_SET_MASTER_ON ((SetMasterOnFn)0x8d5042c0u)
typedef void* (*ControllerFn)();
#define STOCK_MIXER_CONTROLLER ((ControllerFn)0x8c0ba620u)
typedef void (*PickFn)(void* controller, unsigned id);
enum { PICK_WORD = 0x28 };
static volatile u8* const MEMORY_BLOCKS = (volatile u8*)0x849a8a10u;

static const char LOG_FILE[] = "op1f/save_mod.cpp";       // the log shows "save_mod"
static const char KEPT[] = "save_mod";

// One of the mixer's memories, as the pack keeps it.
struct Memory {
    int has;                                              // 1 when it holds something
    int params[MEMORY_VALUES];                            // the upper halves of its eight words
    int subscreen;                                        // 0 to SUBSCREEN_MOST
};

// One set of what the pack keeps.
struct Kept {
    int v[VALUES];                                        // eq, then fx, then master
    int speed;
    int effect;                                           // the unit's effect on the master, 0 to EFFECTS - 1; -1 for none kept
    int active;                                           // the master effect switched on: 1 or 0
    char rack[NAME_ROOM];                                 // the name of the pack's effect riding the master; "" for none
    Memory m[MEMORIES];
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
    u32 retype;                                           // frames left for the base to end a ride, `pending` to write after
    Kept start;                                           // the unit's values at the first frame: a tape with none takes them
    Kept last;                                            // what was last read from or set into the tape
    Kept pending;
    char text[KEPT_ROOM];
} s;

static void say(int level, int line, const char* format, int a = 0, int b = 0) {
    if (s.lines >= (u32)LINES_MOST) return;
    ++s.lines;
    op1f_log(level, LOG_FILE, line, format, a, b);
}

static void say_name(int level, int line, const char* format, const char* name) {
    if (s.lines >= (u32)LINES_MOST) return;
    ++s.lines;
    op1f_log(level, LOG_FILE, line, format, name);
}

extern "C" void save_mod_changed(unsigned where, unsigned which, unsigned how) OP1F_NAMED("save_mod_changed");
extern "C" void save_mod_changed(unsigned where, unsigned which, unsigned how) {
    if (where != (unsigned)OP1F_KEEP_TAPE || which != 0 || how > 31u) return;
    told |= 1u << how;
    if (how == (unsigned)OP1F_KEPT_LOADED || how == (unsigned)OP1F_KEPT_MADE) last_load = how;
}

// ---------------------------------------------------------------- names

static int same_name(const char* a, const char* b) {
    u32 i = 0;
    for (; i < (u32)NAME_ROOM && a[i] && a[i] == b[i]; ++i) {}
    return i == (u32)NAME_ROOM || a[i] == b[i];
}

static void copy_name(char* to, const char* from) {
    u32 i = 0;
    for (; i < (u32)NAME_MOST && from[i]; ++i) to[i] = from[i];
    for (; i < (u32)NAME_ROOM; ++i) to[i] = 0;
}

// 1 for a name of 1 to NAME_MOST characters of a-z, 0-9, '-' and '_': written in the text as it stands.
static int plain(const char* name) {
    u32 n = 0;
    for (; name[n]; ++n) {
        char c = name[n];
        if (n >= (u32)NAME_MOST || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return n > 0;
}

// The name the unit's list gives an effect's type, or 0.
static const char* effect_name(int id) {
    for (int i = 0; i < (int)EFFECTS; ++i) if (EFFECT_NAMES[i].id == id) return EFFECT_NAMES[i].name;
    return 0;
}

// The type of the effect of that name in the unit's list (n characters at name), or -1.
static int effect_id(const char* name, u32 n) {
    for (int i = 0; i < (int)EFFECTS; ++i) {
        const char* e = EFFECT_NAMES[i].name;
        u32 k = 0;
        while (k < n && e[k] && e[k] == name[k]) ++k;
        if (k == n && e[k] == 0) return EFFECT_NAMES[i].id;
    }
    return -1;
}

// The id in the base's list of the pack's effect of that name, or 0.
static unsigned rack_id(const char* name) {
    unsigned count = 0;
    const Op1fRackEntry* list = op1f_rack_list(&op1f_card_save_mod, &count);
    for (unsigned i = 0; list && i < count; ++i)
        if (list[i].effect && same_name(list[i].effect->head.name, name)) return list[i].id;
    return 0;
}

// The name of the pack's effect riding the master now into `out`, or "".
static void rack_now(char* out) {
    copy_name(out, "");
    unsigned id = op1f_rack_at(&op1f_card_save_mod, (unsigned)OP1F_PLACE_MASTER);
    if (id < (unsigned)RACK_ID_FIRST) return;
    unsigned count = 0;
    const Op1fRackEntry* list = op1f_rack_list(&op1f_card_save_mod, &count);
    for (unsigned i = 0; list && i < count; ++i)
        if (list[i].id == id && list[i].effect) {
            copy_name(out, list[i].effect->head.name);
            if (!plain(out)) copy_name(out, "");      // a name the base would not have taken: kept as none
            return;
        }
}

// ---------------------------------------------------------------- the unit's values

static int upper(int word) { return word >> 16; }         // the 16-bit value the unit's own file holds for a word

static volatile int* memory_words(int i) { return (volatile int*)(MEMORY_BLOCKS + MEMORY_BYTES * i); }

static void read_memory(Memory* m, int i) {
    volatile int* w = memory_words(i);
    int sub = w[0];
    m->subscreen = sub >= 0 && sub <= (int)SUBSCREEN_MOST ? sub : 0;
    m->has = MEMORY_BLOCKS[MEMORY_BYTES * i + 4] ? 1 : 0;
    for (int k = 0; k < (int)MEMORY_VALUES; ++k) m->params[k] = upper(w[2 + k]);
}

static void read_unit(Kept* k) {
    for (int i = 0; i < EQ_COUNT; ++i) k->v[i] = upper(EQ_WORDS[i]);
    for (int i = 0; i < FX_COUNT; ++i) k->v[EQ_COUNT + i] = upper(FX_WORDS[i]);
    for (int i = 0; i < MASTER_COUNT; ++i) k->v[EQ_COUNT + FX_COUNT + i] = upper(MASTER_WORDS[i]);
    k->speed = *TAPE_SPEED;
    int e = *FX_TYPE;
    k->effect = e >= 0 && e < (int)EFFECTS ? e : -1;
    k->active = STOCK_MASTER_ON() ? 1 : 0;
    rack_now(k->rack);
    for (int i = 0; i < (int)MEMORIES; ++i) read_memory(&k->m[i], i);
}

// Copied value by value: a copy of the whole struct could become a call of memcpy, which the module does not link.
static void copy_memory(Memory* to, const Memory* from) {
    to->has = from->has;
    for (int k = 0; k < (int)MEMORY_VALUES; ++k) to->params[k] = from->params[k];
    to->subscreen = from->subscreen;
}

static void copy(Kept* to, const Kept* from) {
    for (int i = 0; i < VALUES; ++i) to->v[i] = from->v[i];
    to->speed = from->speed;
    to->effect = from->effect;
    to->active = from->active;
    copy_name(to->rack, from->rack);
    for (int i = 0; i < (int)MEMORIES; ++i) copy_memory(&to->m[i], &from->m[i]);
}

static int same(const Kept* a, const Kept* b) {
    for (int i = 0; i < VALUES; ++i) if (a->v[i] != b->v[i]) return 0;
    if (a->speed != b->speed || a->effect != b->effect || a->active != b->active || !same_name(a->rack, b->rack)) return 0;
    for (int i = 0; i < (int)MEMORIES; ++i) {
        const Memory* x = &a->m[i];
        const Memory* y = &b->m[i];
        if (x->has != y->has || x->subscreen != y->subscreen) return 0;
        for (int k = 0; k < (int)MEMORY_VALUES; ++k) if (x->params[k] != y->params[k]) return 0;
    }
    return 1;
}

// A word set as the unit's own load sets it: the value in the upper half, the lower half 0. Left alone when its upper
// half already holds the value, so that a value that did not change keeps its lower half as the unit left it.
// Answers 1 when the word was written.
static int put_word(volatile int* w, int v) {
    if (upper(*w) == v) return 0;
    *w = (int)((u32)(u16)v << 16);
    return 1;
}

// The master's effect put on as the unit's own load of its settings file puts it on: built for the type, then the type
// word set. The effect's eight values are written after it, as that load has them.
static void put_effect(int type) {
    STOCK_EFFECT_BUILD(BUILD_MASTER, type);
    *FX_TYPE = type;
}

// A pack's effect put on the master as the unit's list puts it on: through the controller's word, where the base
// stands, with the effect's id in the base's list.
static void pick_rack(unsigned id) {
    void* controller = STOCK_MIXER_CONTROLLER();
    if (!controller) return;
    const u32* table = *(const u32* const*)controller;
    PickFn pick = (PickFn)table[PICK_WORD / 4];
    if (pick) pick(controller, id);
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

// A memory written as the unit's own load writes it: the page, the byte that says it holds something, its values.
static void write_memory(int i, const Memory* m) {
    volatile int* w = memory_words(i);
    for (int k = 0; k < (int)MEMORY_VALUES; ++k) put_word(&w[2 + k], m->params[k]);
    if (w[0] != m->subscreen) w[0] = m->subscreen;
    MEMORY_BLOCKS[MEMORY_BYTES * i + 4] = (u8)(m->has ? 1 : 0);
}

// The effect the tape keeps put on the master. Answers 0 when it is on, 1 when a pack's effect riding the master had to
// be ended first by a type the tape does not keep (the base ends a ride when the type word changes): the rest is then
// written once the base has ended it.
static int put_master_effect(const Kept* k) {
    unsigned riding = op1f_rack_at(&op1f_card_save_mod, (unsigned)OP1F_PLACE_MASTER);
    if (k->rack[0]) {
        unsigned id = rack_id(k->rack);
        if (id >= (unsigned)RACK_ID_FIRST) {
            if (id != riding) pick_rack(id);
            return 0;
        }
        say_name(2, 7, "save_mod: the tape's effect '%s' is not here: the unit's effect under it is put on", k->rack);
    }
    if (riding >= (unsigned)RACK_ID_FIRST) {              // a pack's effect rides the master, and the tape keeps none
        if (k->effect >= 0 && k->effect != *FX_TYPE) {
            put_effect(k->effect);                        // another type: the base ends the ride
            return 0;
        }
        int type = k->effect >= 0 ? k->effect : *FX_TYPE;
        if (type < 0 || type >= (int)EFFECTS) return 0;
        put_effect(type == 0 ? 1 : 0);                    // the same type: another one for a frame, so that the base ends it
        return 1;
    }
    if (k->effect >= 0 && k->effect != *FX_TYPE) put_effect(k->effect);
    return 0;
}

static void write_rest(const Kept* k) {
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
    if ((STOCK_MASTER_ON() ? 1 : 0) != k->active) STOCK_SET_MASTER_ON((unsigned)k->active);   // after the effect
    for (int i = 0; i < (int)MEMORIES; ++i) write_memory(i, &k->m[i]);   // after the effect: a pick empties a memory of page 2
}

static void write_unit(const Kept* k) {
    if (put_master_effect(k)) {
        copy(&s.pending, k);
        s.retype = (u32)RETYPE_FRAMES;
        return;
    }
    write_rest(k);
}

// A frame while a ride is to end: the rest written once the base has ended it, or after RETYPE_FRAMES frames anyway.
static void retype_frame() {
    if (op1f_rack_at(&op1f_card_save_mod, (unsigned)OP1F_PLACE_MASTER) >= (unsigned)RACK_ID_FIRST && --s.retype) return;
    s.retype = 0;
    int type = s.pending.effect;
    if (type >= 0 && type != *FX_TYPE) put_effect(type);
    write_rest(&s.pending);
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

static int word_at(const char* p, const char* word) {
    u32 i = 0;
    for (; word[i]; ++i) if (p[i] != word[i]) return 0;
    return 1;
}

// A switch: a whole number (anything but 0 is 1), true or false.
static const char* flag(const char* p, int* out) {
    if (word_at(p, "true")) { *out = 1; return p + 4; }
    if (word_at(p, "false")) { *out = 0; return p + 5; }
    int v;
    p = number(p, &v);
    if (p) *out = v ? 1 : 0;
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

// A text of 0 to NAME_MOST characters with nothing to escape, at p, its quotes around it: `out` holds it, ended by 0.
static const char* short_text(const char* p, char* out) {
    if (*p != '"') return 0;
    const char* t = ++p;
    while (*p && *p != '"' && *p != '\\') ++p;
    if (*p != '"' || (u32)(p - t) > (u32)NAME_MOST) return 0;
    u32 n = (u32)(p - t);
    for (u32 i = 0; i < (u32)NAME_ROOM; ++i) out[i] = i < n ? t[i] : 0;
    return p + 1;
}

static int named(const char* name, u32 n, const char* word) {
    u32 i = 0;
    for (; i < n; ++i) if (word[i] != name[i]) return 0;
    return word[i] == 0;
}

// A member's name and its colon at p: `name` and `n` set. 0 when there is none.
static const char* member(const char* p, const char** name, u32* n) {
    if (*p != '"') return 0;
    *name = ++p;
    while (*p && *p != '"') ++p;
    if (*p != '"') return 0;
    *n = (u32)(p - *name);
    p = skip(p + 1);
    if (*p != ':') return 0;
    return skip(p + 1);
}

// A memory: an object of exactly has_data, params and subscreen, in any order. 0 when it is not of that form.
static const char* memory_text(const char* p, Memory* m) {
    if (*p != '{') return 0;
    p = skip(p + 1);
    u32 have = 0;
    for (;;) {
        const char* name;
        u32 n;
        p = member(p, &name, &n);
        if (!p) return 0;
        if (named(name, n, "has_data") && !(have & 1u)) { p = flag(p, &m->has); have |= 1u; }
        else if (named(name, n, "params") && !(have & 2u)) { p = list(p, m->params, MEMORY_VALUES); have |= 2u; }
        else if (named(name, n, "subscreen") && !(have & 4u)) {
            int v;
            p = number(p, &v);
            if (p) m->subscreen = v >= 0 && v <= (int)SUBSCREEN_MOST ? v : 0;
            have |= 4u;
        }
        else return 0;
        if (!p) return 0;
        p = skip(p);
        if (*p == ',') { p = skip(p + 1); continue; }
        if (*p != '}' || have != 7u) return 0;
        return p + 1;
    }
}

enum { HAVE_EQ = 1, HAVE_FX = 2, HAVE_MASTER = 4, HAVE_SPEED = 8, HAVE_VERSION = 16, HAVE_TYPE = 32, HAVE_ACTIVE = 64,
       HAVE_RACK = 128, HAVE_M1 = 256, HAVE_M2 = 512 };

// The values out of the member's text, and which members it held (`have`). 1 when its version is 1, 2 or 3, it holds
// the members its version must hold and none it cannot, and each is of its form; 0 otherwise, nothing taken.
//   version 1: eq, fx, master, tape_speed, version
//   version 2: those, and fx_type when the unit's list named the effect
//   version 3: those, fx_active, m1 and m2, and fx_effect when a pack's effect rode the master
// A name the unit's list does not hold in fx_type is as none. A member of another name is a fault.
static int read_kept(const char* t, Kept* k, u32* had) {
    Kept got;
    got.effect = -1;
    got.active = 0;
    copy_name(got.rack, "");
    int version = -1;
    u32 have = 0;
    const char* p = skip(t);
    if (*p != '{') return 0;
    p = skip(p + 1);
    for (;;) {
        const char* name;
        u32 n;
        p = member(p, &name, &n);
        if (!p) return 0;
        u32 bit = 0;
        if (named(name, n, "eq")) { p = list(p, &got.v[0], EQ_COUNT); bit = HAVE_EQ; }
        else if (named(name, n, "fx")) { p = list(p, &got.v[EQ_COUNT], FX_COUNT); bit = HAVE_FX; }
        else if (named(name, n, "fx_active")) { p = flag(p, &got.active); bit = HAVE_ACTIVE; }
        else if (named(name, n, "fx_effect")) {
            p = short_text(p, got.rack);
            if (p && !plain(got.rack)) return 0;
            bit = HAVE_RACK;
        }
        else if (named(name, n, "fx_type")) {
            char type[NAME_ROOM];
            p = short_text(p, type);
            if (p) {
                u32 length = 0;
                while (type[length]) ++length;
                got.effect = effect_id(type, length);     // a name the unit does not know: none
            }
            bit = HAVE_TYPE;
        }
        else if (named(name, n, "m1")) { p = memory_text(p, &got.m[0]); bit = HAVE_M1; }
        else if (named(name, n, "m2")) { p = memory_text(p, &got.m[1]); bit = HAVE_M2; }
        else if (named(name, n, "master")) { p = list(p, &got.v[EQ_COUNT + FX_COUNT], MASTER_COUNT); bit = HAVE_MASTER; }
        else if (named(name, n, "tape_speed")) { p = number(p, &got.speed); bit = HAVE_SPEED; }
        else if (named(name, n, "version")) { p = number(p, &version); bit = HAVE_VERSION; }
        else return 0;
        if (!p || (have & bit)) return 0;                 // a member of no form, or named twice
        have |= bit;
        p = skip(p);
        if (*p == ',') { p = skip(p + 1); continue; }
        if (*p != '}' || *skip(p + 1) != 0) return 0;
        break;
    }
    const u32 base = HAVE_EQ | HAVE_FX | HAVE_MASTER | HAVE_SPEED | HAVE_VERSION;
    const u32 three = HAVE_ACTIVE | HAVE_M1 | HAVE_M2;
    if ((have & base) != base || version < 1 || version > KEPT_VERSION) return 0;
    if (version == 1 && (have & ~base)) return 0;
    if (version == 2 && (have & ~(base | HAVE_TYPE))) return 0;
    if (version == 3 && (have & three) != three) return 0;
    if (got.speed < SPEED_LEAST) got.speed = SPEED_LEAST;   // a file edited by hand: the speed held to its range
    if (got.speed > SPEED_MOST) got.speed = SPEED_MOST;
    copy(k, &got);
    *had = have;
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

static u32 put_memory(char* out, u32 at, const Memory* m) {
    at = put_text(out, at, "{\"has_data\":");
    at = put_number(out, at, m->has);
    at = put_text(out, at, ",\"params\":");
    at = put_list(out, at, m->params, MEMORY_VALUES);
    at = put_text(out, at, ",\"subscreen\":");
    at = put_number(out, at, m->subscreen);
    out[at++] = '}';
    return at;
}

// The member's text, its members in the order of their names, as the base hands it back.
static void text_kept(const Kept* k) {
    char* t = s.text;
    u32 at = put_text(t, 0, "{\"eq\":");
    at = put_list(t, at, &k->v[0], EQ_COUNT);
    at = put_text(t, at, ",\"fx\":");
    at = put_list(t, at, &k->v[EQ_COUNT], FX_COUNT);
    at = put_text(t, at, ",\"fx_active\":");
    at = put_number(t, at, k->active);
    if (k->rack[0] && plain(k->rack)) {
        at = put_text(t, at, ",\"fx_effect\":\"");
        at = put_text(t, at, k->rack);
        t[at++] = '"';
    }
    const char* type = k->effect >= 0 ? effect_name(k->effect) : 0;
    if (type && !plain(type)) type = 0;                   // a name that would need escaping: fx_type is left out
    if (type) {
        at = put_text(t, at, ",\"fx_type\":\"");
        at = put_text(t, at, type);
        t[at++] = '"';
    }
    at = put_text(t, at, ",\"m1\":");
    at = put_memory(t, at, &k->m[0]);
    at = put_text(t, at, ",\"m2\":");
    at = put_memory(t, at, &k->m[1]);
    at = put_text(t, at, ",\"master\":");
    at = put_list(t, at, &k->v[EQ_COUNT + FX_COUNT], MASTER_COUNT);
    at = put_text(t, at, ",\"tape_speed\":");
    at = put_number(t, at, k->speed);
    at = put_text(t, at, ",\"version\":3}");
    t[at] = 0;
}

// ---------------------------------------------------------------- the tape

static void tape_loaded() {
    int got = op1f_value_get(&op1f_card_save_mod, OP1F_KEEP_TAPE, 0, KEPT, s.text, sizeof s.text);
    if (got == OP1F_NOT_NOW) return;                      // asked again at the next frame, nothing said
    s.load_waits = 0;
    s.retype = 0;                                         // a ride still to end for the tape before: this tape's values instead
    if (got < 0 && got != OP1F_NO_FILE) {
        s.tape = 0;
        say(2, 1, "save_mod: the base does not keep the values with the tape (%d): nothing is kept", got);
        return;
    }
    s.tape = 1;
    Kept k;
    u32 have = 0;
    if (got >= 0 && read_kept(s.text, &k, &have)) {
        Kept now;
        read_unit(&now);
        if (!(have & HAVE_ACTIVE)) {                     // forms 1 and 2: what they do not hold
            k.active = s.start.active;                    // the switch: the start's
            copy_name(k.rack, now.rack);                  // a pack's effect riding the master: as it is
            for (int i = 0; i < (int)MEMORIES; ++i) copy_memory(&k.m[i], &now.m[i]);   // M1 and M2: as they are
        }
        if (k.effect < 0 && !k.rack[0]) {                 // no effect kept: the effect on stays, and its values with it,
            k.effect = now.effect;                        // since the tape's may be another effect's
            copy_name(k.rack, now.rack);
            for (int i = 0; i < FX_COUNT; ++i) k.v[EQ_COUNT + i] = now.v[EQ_COUNT + i];
        }
        write_unit(&k);
        copy(&s.last, &k);
        s.held = 1;
        say(3, 2, "save_mod: the tape's values taken, tape speed %d", k.speed);
        return;
    }
    if (got >= 0) say(2, 3, "save_mod: the tape's value is not of form 1, 2 or 3: the values of the start are taken");
    write_unit(&s.start);                                 // a tape with none: the values of the start, kept from now on
    s.held = 0;
}

static void tape_keep() {
    if (!s.tape || s.load_waits || s.retype) return;      // nothing set over a load still to be taken or written
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
    else if (s.retype) retype_frame();
    if ((bits & (1u << OP1F_KEPT_SAVED)) && s.tape) say(3, 6, "save_mod: the values written into the tape");
    tape_keep();
}
