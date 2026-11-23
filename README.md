# save mod.pack

An [open-field](https://github.com/LaP0573/open-field) pack for the OP-1 Field (OS 1.7.3+0, base 30 or later) that keeps **with each tape**:
- the tape speed;
- the settings of Mixer pages 2 (EQ), 3 (FX) and 4 (master compressor);
- the master effect: which one, whether it is on, and a pack's effect riding it;
- the mixer's two memories, M1 and M2.

It changes **no place of the firmware**. It uses level 7 of the base (open-field's `docs/level-7.md`), which keeps a JSON member of the pack's own in the tape's file.

## What it does

Each tape's file gets a member `save_mod`:

```json
"save_mod": {"eq":[0,0,0],"fx":[0,0,0,0,0,0,0,0],"fx_active":1,"fx_effect":"ping","fx_type":"delay",
             "m1":{"has_data":1,"params":[0,32767,23538,32767,0,0,0,0],"subscreen":1},
             "m2":{"has_data":0,"params":[0,0,0,0,0,0,0,0],"subscreen":0},
             "master":[0,0,0,0],"tape_speed":0,"version":3}
```

| Member | What it holds |
|---|---|
| `eq` | Mixer page 2: the master EQ, 3 values |
| `fx` | Mixer page 3: the master effect's 8 values |
| `fx_active` | Mixer page 3: the master effect switched on (1) or off (0) |
| `fx_effect` | a pack's effect riding the master, by its name in the unit's list (for instance a wagon effect of `fx train`). Left out when none rides it |
| `fx_type` | the unit's own effect on the master (the one a pack's effect rides, when one does), by the name `op1.json` gives it: `delay`, `grid`, `punch`, `phone`, `spring`, `mother`, `nitro`, `cwo`, `vocoder`, `terminal`, `fazer` |
| `m1`, `m2` | the mixer's two memories, the M1 and M2 keys under knobs 7 and 8 on the tape and mixer pages. Each holds whether it is set, its 8 values, and the mixer page it brings back (0 levels, 1 EQ, 2 effect, 3 compressor), as `op1.json` keeps them under `trick_1` and `trick_2` |
| `master` | Mixer page 4: the master compressor, L, R, drive, release |
| `tape_speed` | the tape speed, in semitones × 1024, from −24 to +12 semitones |
| `version` | the member's form, 3. Forms 1 and 2, written by earlier versions of the pack, are still read |

The values of `eq`, `fx`, `master` and of the memories' `params` are the same signed 16-bit numbers the unit writes into `op1.json`.

- **A tape is loaded** (a switch to another tape, a new tape or a copy): it takes its own values when it has a `save_mod`.
  - **A pack's effect** (`fx_effect`) is put on the master the way the unit's list puts it on. When that pack is not installed, the unit's own effect under it (`fx_type`) is put on instead, and the log says so once.
  - **One of the unit's own effects** is put on the way the unit's own load of `op1.json` puts it on. A pack's effect riding the master is then taken off. When it rides the very effect the tape keeps, the pack puts another effect on for one frame so that the base takes the ride off, then the tape's.
  - **Then the rest is set:** the 8 effect values, the 4 knob values held to the active effect's ranges, the switch, and M1 and M2.
  - **A tape with no `save_mod`** (an older tape, or a new one) takes the values the unit had at the start, those of `op1.json`, and keeps them from then on.
  - **A tape written by an earlier version** (form 1 or 2) keeps what it holds. A pack's effect on the master and M1/M2 stay as they are, and the switch takes the start's value. A form 1 tape, which keeps no effect, also keeps the active effect and its values.
- **A setting changes:** a value on one of the three pages, the effect or its switch, M1 or M2 set or cleared, or the tape speed. The tape's value follows, and is written into the file when the unit writes the tape. The unit decides when.
- **A file edited by hand:**
  - a tape speed out of range is brought back into `[-24, +12]` semitones;
  - `fx_active` other than 0 counts as 1;
  - a memory's values are held to 16 bits, and a page other than 0 to 3 counts as 0;
  - an effect name the unit does not know leaves the active effect as it is;
  - a member that has none of the three forms (a memory missing one of its three members, say) is ignored as a whole, and the tape takes the values of the start.

## Backward compatibility

The base writes only the member `save_mod` and never touches a member of the unit's own. Without the pack, or without open-field, the firmware reads the file as it always does and ignores `save_mod`. Once the pack is removed, the base still keeps the member with the tape.

Going back from version 4 to version 3 of the pack: version 3 does not read form 3, so tapes written by version 4 then take the values of the start. Nothing is damaged, and the next save under version 3 writes form 2. Version 4 reads all three forms.

## Install

Requirements: OS 1.7.3+0, open-field installed with base 30 or later, `base.pack` in `user/pk/`.

1. Copy `save mod.pack` into `user/pk/`.
2. Eject and reboot. At start-up the log says `keeps: 'save mod' keeps 1 value(s) in tapes`, then `save_mod: the values of the start kept`.

To remove it: delete the file and reboot.

If anything goes wrong, hold the Help key at start-up: the unit starts without any pack, and nothing is erased.

## Compatibility

No place of the firmware is changed, so the pack cannot collide with another pack. Version 3 was tried on a unit together with `fx train`, `lfo`, `master mod`, `sidechain` and `garden`.

- **What it asks of the base:** `op1f_rack_at` and `op1f_rack_list`, the base's list of the packs' effects, which any pack may read.
- **How it puts a pack's effect on the master:** through the mixer controller's word that the base stands in (its `op1f_fx_master_pick`), read as the unit holds it. The pack names no function of the base that the base does not offer. If a later base stops standing in that word, a pack's effect is no longer put back, but nothing else breaks.
- **What happens to a pack's effect already on the master:** when the pack puts one of the unit's own effects on the master, the base takes it off, as it does when the unit, MIDI or `fx train` changes the effect.
- **Choosing an effect clears memories of page 2:** putting a pack's effect on clears any memory of the effect page, as choosing an effect on the unit does. The tape's memories are set right after it.
- **The member's name,** `save_mod`, is the pack's own.
- **Other packs:** another pack that also writes the master EQ, effect, compressor or memories when a tape loads would compete with this one.

## Build

In a clone of open-field with the toolchain installed (`docs/build.md`):

```bash
cp -r <this repository>/examples/save-mod examples/
```

```bash
python -I tools/pack_build.py build3 --recipe examples/save-mod/recipe.json --profile profiles/op1f-1.7.3.json --out "out/save mod.pack"
```

## Status

- **Version 4 of the pack** (member form 3): built and checked with `pack_build.py check`. The member's text, its three forms and the hold to ranges were tested on a computer, and so was the way the effect is put on, against stand-ins for the firmware and the base. **Not yet tried on a unit.**
- **Version 3** (member form 2): tried on a unit (OS 1.7.3+0, open-field 0.12). The tape speed, the EQ, the master effect with its values and the compressor come back with each tape.
- Versions 1 and 2 are not to be used:
  - version 1 did not keep the master effect, and could put one effect's values on another; a freeze was seen with it while changing the effect;
  - version 2 brought back another effect than the one saved.
- `out/save mod.pack` was built with a Blackfin toolchain compiled on Linux from the fork open-xy pins. With that toolchain, open-field's loader and examples come out byte for byte the same as the kept ones; only `base.pack` differs slightly. A build with the Windows reference toolchain may give a different but equivalent file.

## Licence

MIT, see [LICENSE](LICENSE).
