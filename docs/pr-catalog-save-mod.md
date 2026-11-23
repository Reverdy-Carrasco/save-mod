# Pull request: catalog: list save mod

**From:** `Reverdy-Carrasco/green-field`, branch `catalog/save-mod` (one commit on `LaP0573/open-field` `main` at `4f29fa3`).
**To:** `LaP0573/open-field`, `main`.
**Status:** ready, not opened yet.

---

## What it changes, and why

It adds one entry to `catalog/packs.json`, and changes nothing else: `save mod`, listed under `tape` and `mixer`, by Reverdy-Carrasco.

`save mod` keeps with each tape, as a member `save_mod` of the tape's file through level 7 of the base:
- the master EQ;
- the master effect: which one, its eight values, whether it is on, and a pack's effect riding it;
- the master compressor;
- the mixer's two memories M1 and M2;
- the tape speed.

Loading a tape brings its values back. A tape with none takes the values the unit had at the start, those of `op1.json`. The pack changes no place of the firmware (`places: 0`) and has a card of level 7.

Source, recipe and the pack file: https://github.com/Reverdy-Carrasco/save-mod. The file is `out/save mod.pack` at commit `177b18a5f412ea7f96d0eea297d5a931a7f8a25b`.

What it asks of the base:
- `op1f_level_7`, `op1f_log`, `op1f_value_get` and `op1f_value_set`;
- `op1f_rack_at` and `op1f_rack_list`, to name a pack's effect riding the master.

It puts a pack's effect back on the master through the mixer controller's word the base stands in (`op1f_fx_master_pick`), read as the unit holds it. It names no function of the base that the base does not offer.

## How to try it

1. Install open-field with base 30 and put `save mod.pack` in `user/pk/`, then restart.
2. The log says `keeps: 'save mod' keeps 1 value(s) in tapes`.
3. Change the tape speed, the EQ, the master effect (its type, values and switch, or a pack's effect on it), the compressor, and set M1 or M2.
4. Load another tape, then come back: the values come back.

The README of the repository lists what each member holds and how earlier forms are read.

## What was tried on a unit

- **The unit:** one OP-1 Field on OS 1.7.3+0, bootloader 1000.1.1, open-field 0.12 with base 30.
- **The packs on it:** `save mod` 4, `fx train` 16, `lfo` 11, `master mod` 1, `sidechain` 10 and `garden` 2.
- **What came back with each tape:** the tape speed, the EQ, the master effect with its values, its switch and a pack's effect riding it, the compressor, and M1/M2.
- **Earlier versions:** versions 1 to 3 were tried along the way. The tapes they wrote are read by version 4.
- **Not tried:**
  - a downgrade from version 4 to version 3;
  - a tape written with the pack, loaded on a unit without it. The base's level 7 keeps the member, and the firmware ignores members it does not know.

## For a new place in the firmware

No place of the firmware is changed: the entry says `places: 0`, and the pack has no table of changes.

## Whose work it builds on

open-field's base and its level 7 and 8 interfaces, and the example `dub` as the model for the values kept with a tape.

## Before sending

- [x] `python -I -m unittest discover -s tools/tests` passes (398 tests, 16 skipped).
- [ ] `python -I tools/build_all.py` says `the same` for every file. Not applicable here: the change touches no pack, the loader or the base. With the Blackfin toolchain used for `save mod`, compiled on Linux from the fork open-xy pins, the loader and the examples come out the same and `base.pack` differs slightly, because the toolchain is not the Windows reference one.
- [x] `node builder/validate-devmode.mjs` passes (21 checks).
- [x] `python -I tools/publish_check.py --forms-only catalog` passes. The pack's repository was also checked against a stock image: nothing of the firmware was found.
- [x] Nothing of the firmware is in the change.
- [x] `python -I tools/catalog_check.py --before <the listing at 4f29fa3> --fetch`: `ok: save mod (listed, its file held)`, `16 entries, all in order`.
