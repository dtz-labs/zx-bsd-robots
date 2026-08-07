#ifndef ROBOTS_GLYPH_STYLES_H
#define ROBOTS_GLYPH_STYLES_H

#include <stdint.h>

#define ROBOTS_GLYPH_STYLE_BSD 0u
#define ROBOTS_GLYPH_STYLE_ROBOT 1u
#define ROBOTS_GLYPH_STYLE_ATARI 2u
#define ROBOTS_GLYPH_STYLE_C64 3u
#define ROBOTS_GLYPH_STYLE_COUNT 4u

#define ROBOTS_GLYPH_ROWS 8u
#define ROBOTS_GLYPH_STYLE_LABEL_BYTES 6u

/*
 * Rows use the same representation as robots_font4x8: the four-pixel row is
 * duplicated into both nibbles.  That lets the direct renderer draw a style
 * glyph in either half of an eight-pixel Spectrum byte without conversion.
 */
typedef struct RobotsGlyphStyle {
    char label[ROBOTS_GLYPH_STYLE_LABEL_BYTES];
    uint8_t robot[ROBOTS_GLYPH_ROWS];
} RobotsGlyphStyle;

extern const RobotsGlyphStyle robots_glyph_styles[ROBOTS_GLYPH_STYLE_COUNT];

/* Out-of-range style numbers deliberately fall back to the original BSD set. */
const RobotsGlyphStyle *robots_glyph_style_get(unsigned char style);

#endif
