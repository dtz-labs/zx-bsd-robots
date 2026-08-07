#ifndef ROBOTS_FONT8X8_H
#define ROBOTS_FONT8X8_H

#include <stdint.h>

#define ROBOTS_FONT8X8_FIRST 32u
#define ROBOTS_FONT8X8_GLYPHS 96u
#define ROBOTS_FONT8X8_BYTES (ROBOTS_FONT8X8_GLYPHS * 8u)

/*
 * Fixed-width 8x8 font for the Timex 512-column renderer.  Bit 7 is the
 * leftmost pixel, matching the byte order used by the Timex display files.
 */
extern const uint8_t robots_font8x8[ROBOTS_FONT8X8_BYTES];

/* The enemy is pixel art, not the printable '+' character. */
extern const uint8_t robots_robot8x8[8];

#endif
