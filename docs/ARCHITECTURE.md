# Architecture

## Portable rules

`src/game.c` owns every rule and contains no Spectrum or z88dk dependency. A
`RobotsGame` stores the player, at most 40 live robots, at most 40 persistent
heaps, level, score, wait state, and a deterministic 16-bit RNG. It uses no
heap allocation.

Robot destinations are calculated before collision resolution, preserving the
original simultaneous movement. Reaching the player takes precedence over a
robot collision on that same square, as it does in the BSD code. A `W` wait is
advanced one turn at a time so the Spectrum front end can animate it.

Host tests construct exact board states and cover:

- unique placement and 10/20/30/40 level population;
- simultaneous collision and persistent heaps;
- rejection of occupied, out-of-bounds, and immediately fatal safe moves;
- deterministic empty-cell teleportation that can still be fatal;
- normal scoring, surviving-wait bonus, and loss of that bonus on death;
- cell queries used by tools and alternate front ends.

## Spectrum front end

z88dk CRT5 supplies a 64×24, 4×8 terminal. The 59×22 logical arena is drawn
at screen columns 1–59 and rows 1–22, with the original frame at columns 0/60
and rows 0/23. Status replaces part of the top border, leaving the play area
unchanged.

The front end builds a character snapshot in global memory and compares it to
the previous snapshot. Only changed field cells are printed after the first
frame, keeping safe runs and risky waits readable. The buffers are global so
they cannot exhaust the Z80 stack.

Two adjacent 4-pixel characters share one Spectrum attribute byte. Attribute
priority is player, robot, heap, border, empty field; this keeps important
objects visible when two characters share a colour cell.

`src/font4x8.c` contains 96 glyphs (ASCII 32–127), eight bytes each. Every
4-bit scanline is duplicated into both nibbles, allowing a glyph to occupy
either half of an 8-pixel Spectrum byte. The font is selected at link time via
`CRT_OTERM_FONT_4X8`.

## 48K gate

The link map, not the tape container size, is authoritative. The automated
gate reads `__BSS_END_tail`, `__register_sp`, and `__crt_stack_size` and fails
unless at least the declared 2 KB stack reserve remains. The TAP validator
also verifies block checksums, BASIC autostart, load address, and CODE bounds.

The emulator smoke test starts ZEsarUX as a 48K machine without video or audio,
decodes the custom 64-column font directly from Spectrum RAM, and exercises
title, game start, help, quit cancellation, and return to play.
