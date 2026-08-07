#include <stdio.h>

#include "font4x8.h"

static const uint8_t *glyph(unsigned char code)
{
    return &robots_font4x8[(unsigned int)(code - ROBOTS_FONT_FIRST) * 8u];
}
static int same_glyph(unsigned char first, unsigned char second)
{
    const uint8_t *left;
    const uint8_t *right;
    unsigned char row;

    left = glyph(first);
    right = glyph(second);
    for (row = 0u; row < 8u; ++row) {
        if (left[row] != right[row])
            return 0;
    }
    return 1;
}

int main(void)
{
    unsigned int i;
    const uint8_t *plus;
    const uint8_t *heap;
    const uint8_t *player;
    const uint8_t *letter_a;
    unsigned char nonzero;

    for (i = 0u; i < ROBOTS_FONT_BYTES; ++i) {
        if ((robots_font4x8[i] >> 4) != (robots_font4x8[i] & 15u)) {
            fprintf(stderr, "font byte %u does not duplicate its nibble\n", i);
            return 1;
        }
    }

    plus = glyph('+');
    heap = glyph('*');
    player = glyph('@');
    letter_a = glyph('A');
    if (letter_a[1] != 0x22u || letter_a[4] != 0x77u ||
        letter_a[6] != 0x55u) {
        fputs("adapted z88dk letter shapes changed unexpectedly\n", stderr);
        return 1;
    }
    if (plus[2] != 0x22u || plus[4] != 0x77u || plus[6] != 0x22u) {
        fputs("printable plus glyph changed unexpectedly\n", stderr);
        return 1;
    }
    if (heap[3] != 0xffu || player[3] != 0xffu) {
        fputs("heap/player pixel-art glyph changed unexpectedly\n", stderr);
        return 1;
    }
    if (same_glyph('+', '*') || same_glyph('+', '@') || same_glyph('*', '@')) {
        fputs("plus, heap, and player glyphs must remain distinct\n", stderr);
        return 1;
    }
    if (same_glyph('+', '{') || same_glyph('+', '}') ||
        same_glyph('+', '[') || same_glyph('+', ']')) {
        fputs("frame corners must not reuse the plus glyph\n", stderr);
        return 1;
    }

    nonzero = 0u;
    for (i = 0u; i < 8u; ++i) {
        if ((robots_robot4x8[i] >> 4) != (robots_robot4x8[i] & 15u)) {
            fputs("robot sprite must duplicate each nibble\n", stderr);
            return 1;
        }
        if (robots_robot4x8[i] != 0u)
            nonzero = 1u;
    }
    if (nonzero == 0u) {
        fputs("robot sprite must not be blank\n", stderr);
        return 1;
    }

    puts("all 4x8 font tests passed");
    return 0;
}
