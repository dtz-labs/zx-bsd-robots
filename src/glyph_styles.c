#include "glyph_styles.h"

#define ROW(bits_) ((uint8_t)((bits_) * 0x11u))
#define GLYPH(a_, b_, c_, d_, e_, f_, g_, h_) \
    { ROW(a_), ROW(b_), ROW(c_), ROW(d_), \
      ROW(e_), ROW(f_), ROW(g_), ROW(h_) }

const RobotsGlyphStyle robots_glyph_styles[ROBOTS_GLYPH_STYLE_COUNT] = {
    {
        "BSD",
        /* The historical terminal '+'. */
        GLYPH(0, 0, 2, 15, 2, 0, 0, 0)
    },
    {
        "ROBOT",
        /* The port's original little antenna-and-legs robot. */
        GLYPH(6, 15, 9, 15, 6, 9, 9, 0)
    },
    {
        "ATARI",
        /* Four-pixel interpretation of the Atari Fuji mark. */
        GLYPH(0, 6, 6, 6, 15, 9, 9, 0)
    },
    {
        "C64",
        /* Commodore C with the two inner bars. */
        GLYPH(0, 7, 8, 11, 11, 8, 7, 0)
    }
};

const RobotsGlyphStyle *robots_glyph_style_get(unsigned char style)
{
    if (style >= ROBOTS_GLYPH_STYLE_COUNT)
        style = ROBOTS_GLYPH_STYLE_BSD;
    return &robots_glyph_styles[style];
}

#undef GLYPH
#undef ROW
