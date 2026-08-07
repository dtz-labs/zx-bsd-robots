#include <stdio.h>

#include "font8x8.h"
#include "timex_themes.h"

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
    const uint8_t *robot;
    const uint8_t *atari;
    const uint8_t *c64;

    if (robots_timex_theme_enemy(ROBOTS_TIMEX_THEME_ORIGINAL) != NULL) {
        fputs("original Timex theme must use the printable plus\n", stderr);
        return 1;
    }
    if (robots_timex_theme_enemy(99u) != NULL) {
        fputs("invalid Timex theme must fall back to original\n", stderr);
        return 1;
    }

    robot = robots_timex_theme_enemy(ROBOTS_TIMEX_THEME_ROBOT);
    atari = robots_timex_theme_enemy(ROBOTS_TIMEX_THEME_ATARI);
    c64 = robots_timex_theme_enemy(ROBOTS_TIMEX_THEME_C64);
    if (robot == NULL || atari == NULL || c64 == NULL ||
        same_rows(robot, atari) || same_rows(robot, c64) ||
        same_rows(atari, c64)) {
        fputs("Timex enemy themes must be present and distinct\n", stderr);
        return 1;
    }
    if (robots_timex_theme_label(ROBOTS_TIMEX_THEME_ORIGINAL)[0] != 'O' ||
        robots_timex_theme_label(ROBOTS_TIMEX_THEME_ROBOT)[0] != 'P' ||
        robots_timex_theme_label(ROBOTS_TIMEX_THEME_ATARI)[0] != 'A' ||
        robots_timex_theme_label(ROBOTS_TIMEX_THEME_C64)[0] != 'C' ||
        robots_timex_theme_label(99u)[0] != 'O') {
        fputs("Timex theme labels or fallback changed unexpectedly\n", stderr);
        return 1;
    }

    puts("all Timex theme tests passed");
    return 0;
}
