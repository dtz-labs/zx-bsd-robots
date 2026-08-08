#include "robots_screen.h"

/*
 * The display file is addressed as
 *
 *     (pixel_y & 0xc0) << 5 | (pixel_y & 0x07) << 8 | (pixel_y & 0x38) << 2 | x
 *
 * With pixel_y = y * 8 + row and y < 24, the three terms separate cleanly:
 * y & 0x18 picks the screen third, y & 0x07 the character row inside it, and
 * row only ever reaches the (pixel_y & 0x07) << 8 term.  Folding that last one
 * out leaves an offset that depends solely on the character cell, which is why
 * the pixel rows can then be stepped with ROBOTS_SCREEN_ROW_STRIDE.
 *
 * Every term is a shift or mask of an 8-bit value, so sdcc never reaches for
 * its 16-bit multiply helper here.
 */
unsigned int robots_screen_cell_offset(unsigned char x, unsigned char y)
{
    return (unsigned int)(((unsigned int)(y & 0x18u) << 8) |
                          ((unsigned int)(y & 0x07u) << 5) |
                          (unsigned int)(x >> 1));
}

unsigned int robots_screen_attribute_offset(unsigned char x, unsigned char y)
{
    return (unsigned int)(((unsigned int)y << 5) | (unsigned int)(x >> 1));
}
