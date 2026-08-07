#ifndef ROBOTS_FONT4X8_H
#define ROBOTS_FONT4X8_H

#include <stdint.h>

#define ROBOTS_FONT_FIRST 32u
#define ROBOTS_FONT_GLYPHS 96u
#define ROBOTS_FONT_BYTES (ROBOTS_FONT_GLYPHS * 8u)

/*
 * Fixed-width 4x8 font for the Spectrum's direct 64-column renderer.
 * Each 4-bit row is duplicated into both nibbles so the same glyph can be
 * drawn in either half of an 8-pixel Spectrum character cell.
 */
extern const uint8_t robots_font4x8[ROBOTS_FONT_BYTES];

/* Fixed enemy sprite.  The printable '+' in the font remains a true plus. */
extern const uint8_t robots_robot4x8[8];

#endif
