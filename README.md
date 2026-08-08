# ZX BSD Robots

**ZX BSD Robots** is a faithful Sinclair ZX port of Ken Arnold's classic BSD
terminal game `robots`. You can
[play it in the browser](https://dtz-labs.github.io/zx-bsd-robots/) or download
either ready-to-load edition directly from the latest GitHub release:

- [ZX Spectrum 48K TAP](https://github.com/dtz-labs/zx-bsd-robots/releases/latest/download/zx-bsd-robots-48k.tap)
  for an unexpanded ZX Spectrum 48K and compatible 128K machines;
- [Timex 512×192 TAP](https://github.com/dtz-labs/zx-bsd-robots/releases/latest/download/zx-bsd-robots-timex-512.tap)
  for the TC2048, TC2068, and TS2068, using the Timex SCLD's real hi-res mode.

The TAP files are built by GitHub Actions and attached individually to
[GitHub Releases](https://github.com/dtz-labs/zx-bsd-robots/releases). They are
not committed to the repository and are not wrapped in ZIP archives.

The browser player can switch between the Spectrum 48K and Timex TC2048
editions without leaving the page. Its TC2048 mode uses a byte-identical copy
of the permitted Spectrum 48K ROM under the filename expected by JSSpeccy3;
no Timex-authored ROM image is published.

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
The Spectrum edition draws the logical `+` as one fixed pixel-art robot. The
Timex edition defaults to the original printable `+` and lets you cycle through
pixel robot, Atari-style, and C64-style enemy themes with `G` on its title
screen. `@` remains the player and `*` remains a junk heap in every theme.
Frame corners have dedicated joined glyphs and never reuse the enemy.

Both editions retain a 64-column display, so the original 61-character arena
fits without scaling. The Spectrum version uses a cleaner 4×8 font adapted
from z88dk's `font_4x8_default` by Dominic Morris, while retaining local object
and frame glyphs. It draws the player in bright red on black; because Spectrum
colour attributes cover an 8×8 cell, the neighbouring 4-pixel character half
shares that colour.
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
| `T` | Teleport to a random empty square |
| `W` | Risky wait until you die or the field is cleared |
| `I` or `?` | On-machine help |
| `Q` | Quit, with confirmation |
| `L` | On the title screen, read the complete two-page BSD licence |
| `G` | Timex title only: cycle ORIGINAL + / PIXEL ROBOT / ATARI / C64 |

The game is keyboard-only. In particular, `0` is not an alias for teleport:
use `T` so the numeric movement layout remains unambiguous.

The title screen shows the complete original and numeric movement-key diagrams.
The on-machine help explains the rules and every in-game key, while the
two-page licence viewer keeps the complete BSD notice, conditions, and
disclaimer available on the machine itself.

## Build and verification

The default `make` target prints help instead of silently choosing a build.

```sh
make spectrum        # build build/zx-bsd-robots-48k.tap
make timex           # build build/zx-bsd-robots-timex-512.tap
make all             # build both TAP files in build/
make test            # host mechanics + TAP + memory-layout gates
make smoke-spectrum  # 48K UI smoke in headless ZEsarUX
make smoke-timex     # 512×192 UI smoke on an emulated TC2048
make verify          # tests and both emulator smoke tests
make bench           # measure board refresh time in emulated milliseconds
make run-spectrum    # launch the 48K edition in ZEsarUX
make run-timex       # launch the hi-res edition as a TC2048
make run             # alias for make run-spectrum
```

Defaults point to the sibling z88dk checkout and the standard macOS ZEsarUX
application. Override `Z88DK_HOME` or `ZESARUX` when they live elsewhere.

Both builds use z88dk `sdcc_iy` with startup 31 and no stdio/CRT terminal.

Both renderers draw incrementally, straight into the display file. A turn is
compared against the previous one cell by cell, and only cells that actually
changed are plotted; untouched cells are never rewritten, which is what keeps
the board from flickering. Within a character cell the eight pixel rows are
256 bytes apart, so `src/screen.c` computes one address per glyph and steps it
— `tests/test_screen.c` checks that arithmetic against the plain ULA formula
for every cell on screen. On the 48K, the player's red highlight is moved one
attribute at a time rather than by repainting all 768.

`make bench` measures what this costs, in emulated milliseconds of Z80 work per
keypress, by sampling the program counter in ZEsarUX against a free-running
t-state counter. A turn currently costs about 12 ms on the 48K and 6 ms on the
Timex — under one 20 ms frame either way.

The linker also emits a map. `tools/check_48k_layout.py` verifies the actual
BSS-to-stack gap; the size of the TAP file alone is not accepted as proof that
the game fits in 48K.

## Layout

```text
include/    portable game and font interfaces
src/game.c  deterministic, platform-independent game rules
src/controls.c  portable keyboard-only command mapping
src/main.c  machine screens, direct renderers, and turn loop
src/font4x8.c  adapted z88dk 4x8 font plus local object/frame glyphs
src/font8x8.c  public-domain-derived Timex hi-res font
src/timex_themes.c  Timex-only enemy theme bitmaps and labels
tests/      host tests for mechanics, controls, fonts, and Timex themes
tools/      TAP/layout checks and ZEsarUX UI smoke tests
site/       GitHub Pages player powered by the dtz-labs JSSpeccy3 fork
.github/    CI, raw-TAP release publishing, Pages deployment, and funding
docs/       upstream and architecture notes
```

## Origin and licence

The game was written by Ken Arnold and appeared in BSD games. This port uses
the NetBSD version as its behavioural reference. See
[docs/UPSTREAM.md](docs/UPSTREAM.md) for the exact sources and documented port
differences.

The game and port source are covered by the 3-clause BSD licence in
[LICENSE](LICENSE). Press `L` on the title screen to read that complete licence
in-game. The Spectrum font adaptation retains Dominic Morris's attribution and
the z88dk [Clarified Artistic License](LICENSES/Clarified-Artistic.txt).
Detailed provenance is in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
Every GitHub release publishes both licence texts and that notice beside the
two TAP files.
