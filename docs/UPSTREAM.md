# Upstream reference

Robots ZX is a new Spectrum implementation of the rules and presentation of
the BSD game `robots`. The behavioural reference is NetBSD release 10:

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
in the repository `LICENSE`, copied alongside the release TAP, and available
in full through the two-page viewer opened with `L` on the title screen.

## Deliberate Spectrum differences

- Unix command-line modes, user score files, and persistent high-score tables
  do not exist on a tape-loaded 48K machine. The port keeps a session high
  score in RAM.
- Numeric command prefixes are omitted because the top-row number keys double
  as the convenient 3×3 movement layout.
- The optional Unix `-a` level-four start and its 600-point advance bonus are
  omitted; every Spectrum game starts at field 1.
- Redraw, terminal-size checks, real-time mode, and autobot mode are platform
  concerns that do not apply to this turn-based Spectrum build.
- The logical game identities remain `@`, `+`, and `*`. The `@` player and `*`
  heap keep fixed pixel-art glyphs; title-only `G` changes the robot rendering
  between the port's `ROBOT`, playful `ATARI` and `C64` interpretations, and
  `BSD`, the historical plus sign. This presentation choice does not change
  rules or the logical board state.
- The frame uses dedicated corner glyphs rather than reusing `+`, and the whole
  interface uses bright-white ink on black paper.
