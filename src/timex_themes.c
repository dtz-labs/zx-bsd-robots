#include <stddef.h>

#include "font8x8.h"
#include "timex_themes.h"

static const uint8_t atari_enemy8x8[8] = {
    0x18u, 0x18u, 0x3cu, 0x5au, 0x5au, 0x99u, 0x99u, 0x00u
};

static const uint8_t c64_enemy8x8[8] = {
    0x3eu, 0x60u, 0xc0u, 0xceu, 0xceu, 0xc0u, 0x60u, 0x3eu
};

const char *robots_timex_theme_label(unsigned char theme)
{
    switch (theme) {
    case ROBOTS_TIMEX_THEME_ROBOT:
        return "PIXEL ROBOT";
    case ROBOTS_TIMEX_THEME_ATARI:
        return "ATARI";
    case ROBOTS_TIMEX_THEME_C64:
        return "C64";
    default:
        return "ORIGINAL +";
    }
}

const uint8_t *robots_timex_theme_enemy(unsigned char theme)
{
    switch (theme) {
    case ROBOTS_TIMEX_THEME_ROBOT:
        return robots_robot8x8;
    case ROBOTS_TIMEX_THEME_ATARI:
        return atari_enemy8x8;
    case ROBOTS_TIMEX_THEME_C64:
        return c64_enemy8x8;
    default:
        return NULL;
    }
}
