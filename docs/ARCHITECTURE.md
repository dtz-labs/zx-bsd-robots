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

The logical robot remains `+` in the portable game state but has one fixed
pixel-art rendering in both editions. The `@` player and `*` heap also have
fixed shapes. There is no runtime object-style selector. The title contains
both movement-key diagrams; title-only `L` opens the complete BSD licence on
two navigable pages.

`src/input.c` normalises z88dk's active-high joystick bits and emits one event
per release and deflection. Enabled joystick input is polled beside the
keyboard and maps FIRE to teleport. Kempston defaults off and is toggled with
title-screen `K`: when the interface is absent, port `$1F` is a floating bus,
so automatic detection would create phantom moves. Cursor (`5/6/7/8/0`) and
Sinclair 1 (`6/7/8/9/0`) share physical keyboard-matrix contacts, so software
cannot identify both
simultaneously. Title-screen `J` selects one matrix convention or turns matrix
joystick decoding off; all non-overlapping keyboard commands remain active.

## Spectrum 256×192 renderer

The Spectrum build implements a direct 64×24, 4×8 renderer. It composes all
6,144 bitmap bytes and 768 attribute bytes in one global 6,912-byte buffer.
Presentation copies the attributes to Spectrum RAM first and then copies the
bitmap. The two transfers are sequential; this is reduced visible redraw, not
an atomic page flip.

Most cells use bright-white ink on black paper. The attribute cell containing
the player uses bright red on black. Since one Spectrum attribute covers 8×8
pixels while the font cells are 4×8, the character in the other half of that
attribute cell necessarily shares the red ink.

A character snapshot is also compared with the previous field snapshot. Only
changed field cells are re-rasterized into the off-screen buffer after the
first frame, while each presentation copies the complete buffer to display
RAM. The display and character buffers are global so they cannot exhaust the
Z80 stack.

`src/font4x8.c` contains 96 glyphs (ASCII 32–127), eight bytes each. Every
4-bit scanline is duplicated into both nibbles, allowing a glyph to occupy
either half of an 8-pixel Spectrum byte. The renderer reads this font directly;
there is no CRT font redirect.

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
