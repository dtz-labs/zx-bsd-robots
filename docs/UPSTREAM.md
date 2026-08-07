# Upstream reference

ZX BSD Robots is a new Sinclair ZX implementation of the rules and
presentation of the BSD game `robots`. The behavioural reference is NetBSD
release 10:

- [`robots(6)` manual](https://man.netbsd.org/robots.6) — rules, scoring, and
  commands.
- [`robots.h`](https://github.com/NetBSD/src/blob/trunk/games/robots/robots.h)
  — the 59×22 field, 10–40 robots, symbols, and score constants.
- [`make_level.c`](https://github.com/NetBSD/src/blob/trunk/games/robots/make_level.c)
  — level population and placement.
- [`move.c`](https://github.com/NetBSD/src/blob/trunk/games/robots/move.c) —
  ordinary movement, safe-move protection, runs, teleportation, and wait.
- [`move_robs.c`](https://github.com/NetBSD/src/blob/trunk/games/robots/move_robs.c)
  — simultaneous pursuit, collisions, heaps, and scoring.
- [`play_level.c`](https://github.com/NetBSD/src/blob/trunk/games/robots/play_level.c)
  — field completion and the surviving-wait bonus.
- [Official source directory](https://cdn.netbsd.org/pub/NetBSD/NetBSD-release-10/src/games/robots/).

The original files carry a 3-clause BSD licence from the Regents of the
University of California. The notice, conditions, and disclaimer are retained
in the repository `LICENSE`, copied alongside the release TAPs, and available
in full through the two-page viewer opened with `L` on the title screen.

## Deliberate machine-port differences

- Unix command-line modes, user score files, and persistent high-score tables
  do not exist on a tape-loaded 48K machine. The port keeps a session high
  score in RAM.
- Numeric command prefixes are omitted because the top-row number keys double
  as the convenient 3×3 movement layout.
- The optional Unix `-a` level-four start and its 600-point advance bonus are
  omitted; every machine game starts at field 1.
- Redraw, terminal-size checks, real-time mode, and autobot mode are platform
  concerns that do not apply to these turn-based machine builds.
- The logical game identities remain `@`, `+`, and `*`. Spectrum shows `+` as
  one fixed pixel robot. Timex defaults to the original printable `+` and has
  three optional title-screen bitmap jokes (pixel robot, Atari-style, C64-style).
  Themes never change rules or logical board state, and imply no affiliation.
- The frame uses dedicated corner glyphs rather than reusing `+`.
- The Spectrum build uses bright-white ink on black paper except for its
  bright-red player. Its 4×8 cells share 8×8 hardware attributes, so the
  adjacent half-cell shares the player's red ink.
- The TC2048/TC2068/TS2068 build uses the Timex SCLD's 512×192 mode. That
  mode's ink/paper selection applies to the whole screen, so the interface is
  monochrome white on black and distinguishes the player by shape.
- Input is keyboard-only. The port supports the original movement keys and a
  Spectrum-friendly numeric layout; teleport is `T` only, not `0`.

## Timex font reference

The Timex edition's printable 8×8 character shapes are adapted from Daniel
Hepper's public-domain [font8x8](https://github.com/dhepper/font8x8)
collection. The repository stores the converted bitmap data needed by the
direct Timex renderer; no font is loaded at runtime.

## Spectrum font reference

The Spectrum edition's printable 4×8 forms are adapted from Dominic Morris's
[`font_4x8_default`](https://github.com/z88dk/z88dk/blob/4d530b6eb779ad0f2a1d13c0bb670cf717477501/libsrc/_DEVELOPMENT/font/font_4x8/font_4x8_default.bin)
at z88dk commit `4d530b6eb779ad0f2a1d13c0bb670cf717477501`. Local
replacements made in August 2026 preserve the recognisable player, junk heap,
horizontal and vertical frame lines, joined corners, and separate enemy
sprite. The original binary SHA-256 is
`342ccc05f79f45a36f3240bdd453f894004dbb646d9b7f5186e996f650eeff46`.
Attribution and modification details are in
[`THIRD_PARTY_NOTICES.txt`](../THIRD_PARTY_NOTICES.txt); the full Clarified
Artistic License is in
[`LICENSES/Clarified-Artistic.txt`](../LICENSES/Clarified-Artistic.txt).
