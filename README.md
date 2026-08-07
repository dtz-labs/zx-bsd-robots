# ZX BSD Robots

**ZX BSD Robots** is a faithful Sinclair ZX port of Ken Arnold's classic BSD
terminal game `robots`. Two ready-to-load editions are included:

- [dist/zx-bsd-robots-48k.tap](dist/zx-bsd-robots-48k.tap) for an unexpanded ZX
  Spectrum 48K and compatible 128K machines;
- [dist/zx-bsd-robots-timex-512.tap](dist/zx-bsd-robots-timex-512.tap) for the
  TC2048, TC2068, and TS2068, using the Timex SCLD's real 512×192 hi-res mode.

You have no weapon. Every robot takes one step towards you after each turn, so
survival means making the machines collide with one another or with the junk
heaps left by earlier collisions. Teleportation is unlimited, random, and not
necessarily safe.

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

The port keeps the historical `@`, `+`, and `*` identities in the game state.
The logical `+` is drawn as one fixed pixel-art robot; `@` is the player and
`*` is a junk heap. Frame corners have their own corner glyphs and never reuse
the robot.

Both editions retain a 64-column display, so the original 61-character arena
fits without scaling. The Spectrum version uses the hand-drawn 4×8 font and
draws the player in bright red on black; because Spectrum colour attributes
cover an 8×8 cell, the neighbouring 4-pixel character half shares that colour.
The Timex version uses an 8×8 font derived from Daniel Hepper's public-domain
[font8x8](https://github.com/dhepper/font8x8) collection. Timex hi-res has one
screen-wide ink/paper choice, so that edition is monochrome white on black and
distinguishes the player by shape rather than colour.

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
| Joystick directions | One safe movement turn |
| Joystick FIRE | Teleport |
| `J` | On the title screen, select `OFF`, `CURSOR`, or `SINCLAIR 1` matrix mapping |
| `K` | On the title screen, enable or disable Kempston input |
| `L` | On the title screen, read the complete two-page BSD licence |

Kempston works alongside the keyboard when enabled with `K`. It defaults off
because port `$1F` floats on a machine without a Kempston interface and cannot
be auto-detected reliably. Cursor and Sinclair 1 joysticks are both
keyboard-matrix standards: Cursor uses `5/6/7/8/0`, while Sinclair 1 uses
`6/7/8/9/0`. Because the shared contacts
are electrically indistinguishable, only one of those two mappings can be
active at a time; select it with `J`. `OFF` preserves the numeric-key movement
layout exactly. Joystick input is release-triggered, so holding a direction
cannot accidentally spend several turns.

The title screen shows the complete original and numeric movement-key diagrams.
The on-machine help explains the rules and every in-game key, while the
two-page licence viewer keeps the complete BSD notice, conditions, and
disclaimer available on the machine itself.

## Build and verification

The default `make` target prints help instead of silently choosing a build.

```sh
make spectrum-dist   # build dist/zx-bsd-robots-48k.tap
make timex-dist      # build dist/zx-bsd-robots-timex-512.tap
make all             # build both TAP files
make test            # host mechanics + TAP + memory-layout gates
make smoke-spectrum  # 48K UI smoke in headless ZEsarUX
make smoke-timex     # 512×192 UI smoke on an emulated TC2048
make verify          # tests and both emulator smoke tests
make run-spectrum    # launch the 48K edition in ZEsarUX
make run-timex       # launch the hi-res edition as a TC2048
make run             # alias for make run-spectrum
```

Defaults point to the sibling z88dk checkout and the standard macOS ZEsarUX
application. Override `Z88DK_HOME` or `ZESARUX` when they live elsewhere.

Both builds use z88dk `sdcc_iy` with startup 31 and no stdio/CRT terminal. The
Spectrum renderer composes the complete 6,912-byte display in a global
off-screen buffer, then copies attributes first and bitmap pixels second. The
Timex renderer composes its two interleaved 6,144-byte display files. These
transfers reduce visible redraw, but are not atomic page flips.

The linker also emits a map. `tools/check_48k_layout.py` verifies the actual
BSS-to-stack gap; the size of the TAP file alone is not accepted as proof that
the game fits in 48K.

## Layout

```text
include/    portable game and font interfaces
src/game.c  deterministic, platform-independent game rules
src/main.c  machine screens, direct renderers, and turn loop
src/input.c  keyboard-matrix and Kempston joystick input
src/font4x8.c  complete hand-drawn Spectrum font
src/font8x8.c  public-domain-derived Timex hi-res font
tests/      host tests for mechanics, fonts, and joystick decoding
tools/      TAP/layout checks and ZEsarUX UI smoke tests
dist/       ready-to-load Spectrum and Timex TAPs plus distribution licence
docs/       upstream and architecture notes
```

## Origin and licence

The game was written by Ken Arnold and appeared in BSD games. This port uses
the NetBSD version as its behavioural reference. See
[docs/UPSTREAM.md](docs/UPSTREAM.md) for the exact sources and documented port
differences.

Source and binary redistribution are covered by the 3-clause BSD licence in
[LICENSE](LICENSE). Press `L` on the title screen to read the same complete
licence in-game. Binary distributions must keep that licence with the TAP.
