#ifndef ROBOTS_TIMEX_THEMES_H
#define ROBOTS_TIMEX_THEMES_H

#include <stdint.h>

typedef enum RobotsTimexTheme {
    ROBOTS_TIMEX_THEME_ORIGINAL = 0,
    ROBOTS_TIMEX_THEME_ROBOT = 1,
    ROBOTS_TIMEX_THEME_ATARI = 2,
    ROBOTS_TIMEX_THEME_C64 = 3,
    ROBOTS_TIMEX_THEME_COUNT = 4
} RobotsTimexTheme;

/*
 * A null enemy glyph means the original printable '+' from the Timex font.
 * All themes keep '@' as the player and '*' as the heap.
 */
const char *robots_timex_theme_label(unsigned char theme);
const uint8_t *robots_timex_theme_enemy(unsigned char theme);

#endif
