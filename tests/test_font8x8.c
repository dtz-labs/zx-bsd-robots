#include <stdio.h>

#include "font8x8.h"

static const uint8_t *glyph(unsigned char code)
{
    return &robots_font8x8[
        (unsigned int)(code - ROBOTS_FONT8X8_FIRST) * 8u];
}

static int same_rows(const uint8_t *left, const uint8_t *right)
{
    unsigned char row;

    for (row = 0u; row < 8u; ++row) {
        if (left[row] != right[row])
            return 0;
    }
    return 1;
}

int main(void)
{
    const uint8_t *letter_a;
    const uint8_t *plus;
    const uint8_t *heap;
    const uint8_t *player;
    const uint8_t *top_left;
    const uint8_t *top_right;
    const uint8_t *bottom_left;
    const uint8_t *bottom_right;
    const uint8_t *horizontal;
    const uint8_t *vertical;

    if (ROBOTS_FONT8X8_BYTES != 768u) {
        fputs("8x8 font must contain all 96 printable ASCII slots\n", stderr);
        return 1;
    }

    letter_a = glyph('A');
    plus = glyph('+');
    heap = glyph('*');
    player = glyph('@');
    if (letter_a[0] != 0x30u || letter_a[1] != 0x78u ||
        letter_a[4] != 0xfcu) {
        fputs("font8x8_basic rows were not stored in bit-7-left order\n",
              stderr);
        return 1;
    }
    if (plus[1] != 0x30u || plus[3] != 0xfcu || plus[4] != 0x30u) {
        fputs("printable plus glyph changed unexpectedly\n", stderr);
        return 1;
    }
    if (same_rows(plus, heap) || same_rows(plus, player) ||
        same_rows(heap, player) || same_rows(plus, robots_robot8x8) ||
        same_rows(heap, robots_robot8x8) ||
        same_rows(player, robots_robot8x8)) {
        fputs("objects and printable plus must remain visually distinct\n",
              stderr);
        return 1;
    }
    if (player[0] != 0x18u || player[3] != 0x7eu ||
        player[6] != 0x42u) {
        fputs("Timex player must remain a recognisable human sprite\n", stderr);
        return 1;
    }
    if (robots_robot8x8[0] != 0x3cu ||
        robots_robot8x8[2] != 0xdbu ||
        robots_robot8x8[3] != 0xffu ||
        robots_robot8x8[6] != 0x42u) {
        fputs("fixed robot pixel art changed unexpectedly\n", stderr);
        return 1;
    }

    bottom_left = glyph('[');
    bottom_right = glyph(']');
    horizontal = glyph('-');
    top_left = glyph('{');
    vertical = glyph('|');
    top_right = glyph('}');
    if (horizontal[3] != 0xffu || vertical[0] != 0x10u ||
        vertical[7] != 0x10u || top_left[3] != 0x1fu ||
        top_left[4] != 0x10u || top_right[3] != 0xf0u ||
        top_right[4] != 0x10u || bottom_left[2] != 0x10u ||
        bottom_left[3] != 0x1fu || bottom_left[4] != 0x00u ||
        bottom_right[2] != 0x10u || bottom_right[3] != 0xf0u ||
        bottom_right[4] != 0x00u) {
        fputs("frame glyphs do not form joined square corners\n", stderr);
        return 1;
    }
    if (same_rows(plus, top_left) || same_rows(plus, top_right) ||
        same_rows(plus, bottom_left) || same_rows(plus, bottom_right)) {
        fputs("frame corners must not reuse the plus glyph\n", stderr);
        return 1;
    }

    puts("all 8x8 font tests passed");
    return 0;
}
