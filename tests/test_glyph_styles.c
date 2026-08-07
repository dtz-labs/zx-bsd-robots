#include <stdio.h>
#include <string.h>

#include "font4x8.h"
#include "glyph_styles.h"

static int same_rows(const uint8_t *left, const uint8_t *right)
{
    unsigned char row;

    for (row = 0u; row < ROBOTS_GLYPH_ROWS; ++row) {
        if (left[row] != right[row])
            return 0;
    }
    return 1;
}

static const uint8_t *font_glyph(unsigned char character)
{
    return &robots_font4x8[
        (unsigned int)(character - ROBOTS_FONT_FIRST) * ROBOTS_GLYPH_ROWS];
}

int main(void)
{
    const RobotsGlyphStyle *style;
    unsigned char first;
    unsigned char second;
    unsigned char row;

    if (strcmp(robots_glyph_styles[ROBOTS_GLYPH_STYLE_BSD].label, "BSD") ||
        strcmp(robots_glyph_styles[ROBOTS_GLYPH_STYLE_ROBOT].label, "ROBOT") ||
        strcmp(robots_glyph_styles[ROBOTS_GLYPH_STYLE_ATARI].label, "ATARI") ||
        strcmp(robots_glyph_styles[ROBOTS_GLYPH_STYLE_C64].label, "C64")) {
        fputs("glyph style labels changed unexpectedly\n", stderr);
        return 1;
    }

    for (first = 0u; first < ROBOTS_GLYPH_STYLE_COUNT; ++first) {
        style = robots_glyph_style_get(first);
        for (row = 0u; row < ROBOTS_GLYPH_ROWS; ++row) {
            if ((style->robot[row] >> 4) != (style->robot[row] & 15u)) {
                fprintf(stderr, "style %u has a non-duplicated row\n", first);
                return 1;
            }
        }
    }

    if (robots_glyph_styles[ROBOTS_GLYPH_STYLE_BSD].robot[2] != 0x22u ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_BSD].robot[3] != 0xffu ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_ROBOT].robot[0] != 0x66u ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_ROBOT].robot[6] != 0x99u ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_ATARI].robot[4] != 0xffu ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_ATARI].robot[5] != 0x99u ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_C64].robot[1] != 0x77u ||
        robots_glyph_styles[ROBOTS_GLYPH_STYLE_C64].robot[3] != 0xbbu) {
        fputs("a signature row of a named style changed unexpectedly\n",
              stderr);
        return 1;
    }

    for (first = 0u; first < ROBOTS_GLYPH_STYLE_COUNT; ++first) {
        for (second = (unsigned char)(first + 1u);
             second < ROBOTS_GLYPH_STYLE_COUNT; ++second) {
            if (same_rows(robots_glyph_styles[first].robot,
                          robots_glyph_styles[second].robot)) {
                fprintf(stderr, "styles %u and %u share a robot\n",
                        first, second);
                return 1;
            }
        }
    }

    style = robots_glyph_style_get(ROBOTS_GLYPH_STYLE_BSD);
    if (!same_rows(style->robot, font_glyph('+'))) {
        fputs("BSD style must use the printable plus glyph\n", stderr);
        return 1;
    }

    style = robots_glyph_style_get(255u);
    if (style != &robots_glyph_styles[ROBOTS_GLYPH_STYLE_BSD]) {
        fputs("invalid style must fall back to BSD\n", stderr);
        return 1;
    }

    for (first = 0u; first < ROBOTS_GLYPH_STYLE_COUNT; ++first) {
        if (same_rows(robots_glyph_styles[first].robot, font_glyph('{')) ||
            same_rows(robots_glyph_styles[first].robot, font_glyph('}')) ||
            same_rows(robots_glyph_styles[first].robot, font_glyph('[')) ||
            same_rows(robots_glyph_styles[first].robot, font_glyph(']'))) {
            fprintf(stderr, "style %u robot collides with a frame corner\n",
                    first);
            return 1;
        }
    }

    puts("all glyph style tests passed");
    return 0;
}
