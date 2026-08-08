#include <stdio.h>

#include "robots_screen.h"

static int failures;

static void check(int condition, const char *description)
{
    if (!condition) {
        ++failures;
        printf("FAIL: %s\n", description);
    }
}

/*
 * The ULA scatters the eight pixel rows of a character cell across the display
 * file.  This is the plain, obviously-correct formula the game used to call
 * once per pixel row; the module under test has to agree with it exactly.
 */
static unsigned int reference_offset(unsigned char x_byte, unsigned char pixel_y)
{
    return (unsigned int)(((unsigned int)(pixel_y & 0xc0u) << 5) |
                          ((unsigned int)(pixel_y & 0x07u) << 8) |
                          ((unsigned int)(pixel_y & 0x38u) << 2) |
                          x_byte);
}

static void test_cell_offset_matches_reference(void)
{
    unsigned char x;
    unsigned char y;
    unsigned char row;
    unsigned int expected;
    unsigned int actual;

    for (y = 0u; y < ROBOTS_SCREEN_ROWS; ++y) {
        for (x = 0u; x < ROBOTS_SCREEN_COLUMNS; ++x) {
            for (row = 0u; row < 8u; ++row) {
                expected = reference_offset((unsigned char)(x >> 1),
                                            (unsigned char)(y * 8u + row));
                actual = robots_screen_cell_offset(x, y) +
                         (unsigned int)row * ROBOTS_SCREEN_ROW_STRIDE;
                if (expected != actual) {
                    ++failures;
                    printf("FAIL: cell (%u,%u) row %u: expected %u, got %u\n",
                           (unsigned)x, (unsigned)y, (unsigned)row,
                           expected, actual);
                    return;
                }
            }
        }
    }
}

/* Stepping by the stride must stay inside the 6144-byte display file. */
static void test_cell_offsets_stay_in_the_display_file(void)
{
    unsigned char x;
    unsigned char y;
    unsigned int last;

    for (y = 0u; y < ROBOTS_SCREEN_ROWS; ++y) {
        for (x = 0u; x < ROBOTS_SCREEN_COLUMNS; ++x) {
            last = robots_screen_cell_offset(x, y) +
                   7u * ROBOTS_SCREEN_ROW_STRIDE;
            check(last < 6144u, "last pixel row of a cell is inside the file");
        }
    }
}

/* Two adjacent 4x8 columns share one display-file byte on the 48K. */
static void test_adjacent_columns_share_a_byte(void)
{
    check(robots_screen_cell_offset(0u, 0u) == robots_screen_cell_offset(1u, 0u),
          "columns 0 and 1 share a byte");
    check(robots_screen_cell_offset(62u, 23u) ==
              robots_screen_cell_offset(63u, 23u),
          "columns 62 and 63 share a byte");
    check(robots_screen_cell_offset(0u, 0u) != robots_screen_cell_offset(2u, 0u),
          "columns 0 and 2 do not share a byte");
}

/* The attribute for a cell is one byte per 8x8 block, 32 per row. */
static void test_attribute_offset(void)
{
    check(robots_screen_attribute_offset(0u, 0u) == 0u,
          "top-left attribute is the first one");
    check(robots_screen_attribute_offset(1u, 0u) == 0u,
          "columns 0 and 1 share an attribute");
    check(robots_screen_attribute_offset(2u, 0u) == 1u,
          "column 2 uses the next attribute");
    check(robots_screen_attribute_offset(0u, 1u) == 32u,
          "the next character row is 32 attributes on");
    check(robots_screen_attribute_offset(63u, 23u) == 767u,
          "bottom-right attribute is the last one");
}

int main(void)
{
    test_cell_offset_matches_reference();
    test_cell_offsets_stay_in_the_display_file();
    test_adjacent_columns_share_a_byte();
    test_attribute_offset();

    if (failures != 0) {
        printf("%d screen test(s) failed\n", failures);
        return 1;
    }
    printf("screen tests passed\n");
    return 0;
}
