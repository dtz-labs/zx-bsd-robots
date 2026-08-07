# Robots ZX

**Robots ZX** is a faithful ZX Spectrum 48K port of Ken Arnold's classic BSD
terminal game `robots`.

You have no weapon. Every robot takes one step towards you after each turn, so
survival means making the machines collide with one another or with the junk
heaps left by earlier collisions. Teleportation is unlimited, random, and not
necessarily safe.

The release image is [dist/robots-zx-48k.tap](dist/robots-zx-48k.tap). It runs
on an unexpanded ZX Spectrum 48K and compatible 128K machines.

## What was preserved

- The original 59×22 playable field, surrounded by its 61×24 frame.
- 10 robots on field 1, 20 on field 2, 30 on field 3, then 40 forever.
- Simultaneous eight-direction robot pursuit.
- Permanent junk heaps, 10 points per destroyed robot, and unlimited risky
  teleportation.
- Typo protection: an ordinary move that would be eaten on the next robot turn
  is rejected.
- Safe directional runs, safe standing, and the committed `W` wait command.
- The original wait bonus: one extra point per robot destroyed after `W`, but
  only if the player survives the field.

The Spectrum port keeps the historical `@`, `+`, and `*` character codes, but
its hand-drawn 4×8 font renders them as tiny player, robot, and debris sprites.
The 64-column display fits the original arena without scaling: 61 characters
occupy 244 of the Spectrum's 256 horizontal pixels.

## Controls

| Keys | Action |
| --- | --- |
| `Y K U` / `H . L` / `B J N` | Original eight-direction movement and safe stand |
| `7 8 9` / `4 5 6` / `1 2 3` | Spectrum-friendly equivalent layout |
| `Space`, `.`, `5` | Stand still for one safe turn |
| Caps Shift + direction | Run safely in that direction |
| `S` or `>` | Stand safely for as long as possible |
| `T` or `0` | Teleport to a random empty square |
| `W` | Risky wait until you die or the field is cleared |
| `I` or `?` | On-machine help |
| `Q` | Quit, with confirmation |

The on-machine help screen explains the rules and every key; no external
manual is required to start playing.

## Build and verification

The default `make` target prints help instead of silently choosing a build.

```sh
make spectrum-dist   # build dist/robots-zx-48k.tap
make test            # host mechanics + TAP + 48K memory-layout gates
make smoke-spectrum  # title/help/input smoke in headless ZEsarUX
make verify          # all of the above
make run-spectrum    # launch on a Spectrum 48K in ZEsarUX
```

Defaults point to the sibling z88dk checkout and the standard macOS ZEsarUX
application. Override `Z88DK_HOME` or `ZESARUX` when they live elsewhere.

The build uses z88dk's `sdcc_iy` CRT5: a 64×24 terminal with a 4×8 font. It
also emits a link map. `tools/check_48k_layout.py` verifies the actual BSS to
stack gap; the size of the TAP file alone is not accepted as proof that the
game fits in 48K.

## Layout

```text
include/    portable game and font interfaces
src/game.c  deterministic, platform-independent game rules
src/main.c  Spectrum input, screens, colours, and turn loop
src/font4x8.c  complete hand-drawn 96-glyph font
tests/      host tests for the mechanics
tools/      TAP/layout checks and ZEsarUX remote smoke test
dist/       ready-to-load 48K TAP and binary-distribution licence
docs/       upstream and architecture notes
```

## Origin and licence

The game was written by Ken Arnold and appeared in BSD games. This port uses
the NetBSD version as its behavioural reference. See
[docs/UPSTREAM.md](docs/UPSTREAM.md) for the exact sources and documented port
differences.

Source and binary redistribution are covered by the 3-clause BSD licence in
[LICENSE](LICENSE). Binary distributions must keep that licence with the TAP.
