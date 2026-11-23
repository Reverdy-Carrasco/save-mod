# save mod.pack

An [open-field](https://github.com/LaP0573/open-field) pack for the OP-1 Field (OS 1.7.3+0, a base of level 7 or later) that keeps the tape speed and the settings of Mixer pages 2 (EQ), 3 (FX) and 4 (master compressor) **with each tape**.

It changes **no place of the firmware**. It uses level 7 of the base (open-field's `docs/level-7.md`), which keeps a JSON member of the pack's own in the tape's file.

## What it does

Each tape's file gets a member `save_mod`:

```json
"save_mod": {"eq":[0,0,0],"fx":[0,0,0,0,0,0,0,0],"fx_type":"delay","master":[0,0,0,0],"tape_speed":0,"version":2}
```

| Member | What it holds |
|---|---|
| `eq` | Mixer page 2: the master EQ, 3 values |
| `fx` | Mixer page 3: the master effect's 8 values |
| `fx_type` | Mixer page 3: the active master effect, by the name `op1.json` gives it (`delay`, `grid`, `punch`, `phone`, `spring`, `mother`, `nitro`, `cwo`, `vocoder`, `terminal`, `fazer`) |
| `master` | Mixer page 4: the master compressor, L, R, drive, release |
| `tape_speed` | the tape speed, in semitones × 1024, from −24 to +12 semitones |
| `version` | the member's form, 2. Form 1, with no `fx_type`, is still read: the active effect then stays as it is |

The values of `eq`, `fx` and `master` are the same signed 16-bit numbers the unit writes into `op1.json`.

- **A tape is loaded** (a switch to another tape, a new tape or a copy): it takes its own values when it has a `save_mod`.
  - If its master effect is not the active one, the pack puts it on the way the unit's own load of `op1.json` does, then sets the tape's 8 effect values.
  - The 4 knob values are held to the active effect's ranges.
  - A tape with no `fx_type` (values of form 1) keeps the active effect and its values: its own effect values may belong to another effect.
  - A tape with no `save_mod` (an older tape, or a new one) takes the values the unit had at the start, those of `op1.json`, and keeps them from then on.
- **A setting changes** on one of the three pages, or the tape speed changes: the tape's value follows. It is written into the file when the unit writes the tape. The unit decides when.
- **A file edited by hand:**
  - a tape speed out of range is brought back into `[-24, +12]` semitones;
  - an effect name the unit does not know leaves the active effect as it is;
  - a member that has neither form 1 nor form 2 is ignored, and the tape takes the values of the start.

## Backward compatibility

The base writes only the member `save_mod` and never touches a member of the unit's own. Without the pack, or without open-field, the firmware reads the file as it always does and ignores `save_mod`. Once the pack is removed, the base still keeps the member with the tape.

## Install

Requirements: OS 1.7.3+0, open-field installed, `base.pack` in `user/pk/`.

1. Copy `save mod.pack` into `user/pk/`.
2. Eject and reboot. At start-up the log says `keeps: 'save mod' keeps 1 value(s) in tapes`, then `save_mod: the values of the start kept`.

To remove it: delete the file and reboot.

If anything goes wrong, hold the Help key at start-up: the unit starts without any pack, and nothing is erased.

## Compatibility

No place of the firmware is changed, so the pack cannot collide with another pack. Tried on a unit together with `fx train`, `lfo`, `master mod`, `sidechain` and `garden`.

- When the pack changes the master effect, the base takes off a pack's effect riding the master, as it does when the unit, MIDI or `fx train` changes the effect.
- A pack's effect on the master is not kept with the tape.
- The member's name, `save_mod`, is the pack's own.
- Another pack that also writes the master EQ, effect or compressor values when a tape loads would compete with this one.

## Build

In a clone of open-field with the toolchain installed (`docs/build.md`):

```bash
cp -r <this repository>/examples/save-mod examples/
```

```bash
python -I tools/pack_build.py build3 --recipe examples/save-mod/recipe.json --profile profiles/op1f-1.7.3.json --out "out/save mod.pack"
```

## Status

- **Version 3 of the pack** (member form 2): tried on a unit (OS 1.7.3+0, open-field 0.12). The tape speed, the EQ, the master effect with its values and the compressor come back with each tape.
- Versions 1 and 2 are not to be used:
  - version 1 did not keep the master effect, and could put one effect's values on another; a freeze was seen with it while changing the effect;
  - version 2 brought back another effect than the one saved.

  The tapes they wrote are read by version 3.
- `out/save mod.pack` was built with a Blackfin toolchain compiled on Linux from the fork open-xy pins. With that toolchain, open-field's loader and examples come out byte for byte the same as the kept ones; only `base.pack` differs slightly. A build with the Windows reference toolchain may give a different but equivalent file.

## Licence

MIT, see [LICENSE](LICENSE).
