#ifndef ROBOTS_SCREEN_H
#define ROBOTS_SCREEN_H

/* The 4x8 (48K) and 8x8 (Timex hi-res) grids are both 64x24 characters. */
#define ROBOTS_SCREEN_COLUMNS 64
#define ROBOTS_SCREEN_ROWS 24

/*
 * Within one character cell the ULA keeps the eight pixel rows exactly 256
 * bytes apart, because bits 8..10 of a display-file address hold pixel_y & 7.
 * Plotting a glyph is therefore one address calculation followed by seven
 * additions, instead of eight full offset computations.
 */
#define ROBOTS_SCREEN_ROW_STRIDE 256u

/* Offset of a character cell's first pixel row from the display-file base. */
unsigned int robots_screen_cell_offset(unsigned char x, unsigned char y);

/* Offset of a character cell's attribute from the attribute-file base. */
unsigned int robots_screen_attribute_offset(unsigned char x, unsigned char y);

#endif
