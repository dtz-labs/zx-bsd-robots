# Architecture

## Portable rules

`src/game.c` owns every rule and contains no Spectrum or z88dk dependency. A
`RobotsGame` stores the player, at most 40 live robots, at most 40 persistent
heaps, level, score, wait state, and a deterministic 16-bit RNG. It uses no
heap allocation.

Robot destinations are calculated before collision resolution, preserving the
original simultaneous movement. Reaching the player takes precedence over a
robot collision on that same square, as it does in the BSD code. A `W` wait is
advanced one turn at a time so the machine front end can animate it.

Host tests construct exact board states and cover:

- unique placement and 10/20/30/40 level population;
- simultaneous collision and persistent heaps;
- rejection of occupied, out-of-bounds, and immediately fatal safe moves;
- deterministic empty-cell teleportation that can still be fatal;
- normal scoring, surviving-wait bonus, and loss of that bonus on death;
- cell queries used by tools and alternate front ends.

## Shared front end

Both executables use z88dk `sdcc_iy` startup 31, without stdio or a CRT
terminal. `src/main.c` owns their shared 64×24 character layout, screens,
keyboard handling, and turn loop. The 59×22 logical arena is drawn at screen
columns 1–59 and rows 1–22, with its 61×24 frame at columns 0/60 and rows 0/23.
Status replaces part of the top border, leaving the play area unchanged.
Dedicated top-left, top-right, bottom-left, and bottom-right glyphs keep frame
corners independent from game objects.

The logical robot remains `+` in the portable game state. The Spectrum edition
uses one fixed pixel-art rendering. The Timex title offers four presentation
themes: original printable `+`, pixel robot, Atari-style, and C64-style. This
choice changes only the enemy bitmap; the `@` player, `*` heap, rules, and board
state remain unchanged. Both titles contain the two movement-key diagrams;
title-only `L` opens the complete BSD licence on two navigable pages.

Input is keyboard-only. The portable decoder in `src/controls.c` maps keys to
actions, and `src/main.c` dispatches those actions. Both the original
`YKU/H.L/BJN` layout and the Spectrum-friendly `789/456/123` layout are always
available. Teleport is deliberately bound only to `T`; `0` has no command and
there is no joystick polling or title-screen input selector.

## Spectrum 256×192 renderer

The Spectrum build implements a direct 64×24, 4×8 renderer that writes into
display RAM. There is no shadow copy of the screen: a turn changes a handful
of cells, so composing off-screen meant writing each changed byte twice and
then blitting all 6,912 bytes to reveal it. What prevents flicker is not a
buffer but the diff — a cell that did not change is never rewritten.

Each turn is rasterized into a character snapshot, which is compared with the
previous turn's; only differing cells are plotted. `src/screen.c` turns a
character position into a display-file offset once per glyph, and the eight
pixel rows are then reached by stepping 256 bytes, which is the distance the
ULA's interleaved layout puts between the rows of one cell.
`tests/test_screen.c` verifies that arithmetic against the plain formula for
every cell on screen.

The snapshots are one-dimensional. Indexed as `[y][x]` over a 59-wide field
they forced sdcc to synthesize a multiply on every access, which dominated the
redraw; a flat array is walked with pointers instead.

Most cells use bright-white ink on black paper. The attribute cell containing
the player uses bright red on black. Since one Spectrum attribute covers 8×8
pixels while the font cells are 4×8, the character in the other half of that
attribute cell necessarily shares the red ink. Moving the player clears one
attribute and sets one, rather than repainting all 768.

The character snapshots are global so they cannot exhaust the Z80 stack.

`src/font4x8.c` contains 96 glyphs (ASCII 32–127), eight bytes each. Its
printable forms are adapted from Dominic Morris's z88dk
`font_4x8_default`; the object and joined-frame glyphs are local. Every 4-bit
scanline is duplicated into both nibbles, allowing a glyph to occupy either
half of an 8-pixel Spectrum byte. The renderer reads this font directly; there
is no CRT font redirect.

## Timex 512×192 renderer

The Timex build selects the SCLD 512×192 monochrome mode and uses both
6,144-byte display files. Successive 8-pixel character columns alternate
between the files at `$4000` and `$6000`, forming one 512-pixel-wide image;
they are not page-flipped screens. The renderer preserves the Spectrum bitmap
line ordering within each file and restores the normal display mode before
returning to BASIC.

The Timex hi-res palette is global rather than attribute-cell based. The build
therefore uses white ink on black paper throughout and distinguishes the player
by its glyph instead of colour. Printable characters use an 8×8 font adapted
from Daniel Hepper's public-domain
[font8x8](https://github.com/dhepper/font8x8) collection.
`src/timex_themes.c` supplies the three optional enemy bitmaps; a null theme
bitmap deliberately selects the printable `+` for the default original look.

## 48K gate

Each link map, not the tape container size, is authoritative. The automated
gate reads `__BSS_END_tail`, `__register_sp`, and `__crt_stack_size` and fails
unless at least the declared 2 KB stack reserve remains. The TAP validator
also verifies block checksums, BASIC autostart, load address, and CODE bounds
for both editions.

The Spectrum emulator smoke test starts ZEsarUX as a 48K machine without video
or audio, decodes the custom 64-column font directly from RAM, and exercises
the title, both licence pages, game start, help, quit cancellation, and return
to play. It also checks black paper, the player's red attribute, and dedicated
frame corners. The Timex smoke test runs a TC2048, reads back SCLD port `$FF`
to require mode `$3E`, reconstructs the 512×192 image from both display files,
exercises the corresponding hi-res UI flow, and requires a clean return to ULA
mode `$00` on exit.
