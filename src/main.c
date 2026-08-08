#include <input.h>
#include <string.h>
#include <z80.h>

#ifdef ROBOTS_TIMEX_HIRES
#include "font8x8.h"
#include "timex_themes.h"
#else
#include "font4x8.h"
#endif
#include "robots_controls.h"
#include "robots_game.h"
#include "robots_screen.h"

#define SCREEN_COLUMNS 64u
#define SCREEN_ROWS 24u
#define FIELD_LEFT 1u
#define FIELD_RIGHT 60u
#define NOTICE_COLUMN 62u

#define SCREEN_BITMAP_BYTES 6144u
#define SCREEN_ATTRIBUTE_BYTES 768u
#define SCREEN_BITMAP ((unsigned char *)16384u)
#ifdef ROBOTS_TIMEX_HIRES
/*
 * Timex hi-res reads two display files at once: even columns come from the
 * file at 16384, odd columns from its twin 8192 bytes higher.
 */
#define SCREEN_PLANE_STRIDE 8192u
#define TIMEX_ULA_ATTRIBUTES ((unsigned char *)22528u)
#define TIMEX_SCLD_PORT 0xffu
#define TIMEX_HIRES_WHITE_ON_BLACK 0x3eu
#define TIMEX_ULA_MODE 0x00u
#define TIMEX_ULA_WHITE_ON_BLACK 0x47u
#else
#define SCREEN_ATTRIBUTES ((unsigned char *)22528u)
#define ZX_BLACK 0u
#define ZX_RED 2u
#define ZX_WHITE 7u
#define ZX_BRIGHT 64u

#define ATTR(ink_, paper_) \
    ((unsigned char)(ZX_BRIGHT | (ink_) | ((paper_) << 3)))

/* No attribute cell is currently painted as the player. */
#define NO_ATTRIBUTE 0xffffu
#endif

#define ANIMATION_DELAY_MS 90u
#define WAIT_DELAY_MS 150u

#define RENDER_CELL_COUNT (ROBOTS_GAME_HEIGHT * ROBOTS_GAME_WIDTH)

static RobotsGame game;
static unsigned long high_score;
#ifdef ROBOTS_TIMEX_HIRES
static unsigned char timex_theme;
#endif

/*
 * Glyphs go straight into the display file.  A shadow copy of the screen was
 * tried first and cost far more than it saved: a turn only ever changes a
 * handful of cells, so composing off-screen meant writing every changed byte
 * twice and then blitting all 6144 (48K) or 12288 (Timex) bytes to show it.
 * What keeps the board from tearing is that untouched cells are never
 * rewritten at all -- render_board() draws only the difference.
 */
static unsigned char render_cells[RENDER_CELL_COUNT];
static unsigned char previous_cells[RENDER_CELL_COUNT];
static unsigned char board_visible;
#ifndef ROBOTS_TIMEX_HIRES
static unsigned int player_attribute = NO_ATTRIBUTE;
#endif

static void screen_put_glyph(unsigned char x, unsigned char y,
                             const unsigned char *glyph)
{
    unsigned char *target;
    unsigned char row;

    if (x >= SCREEN_COLUMNS || y >= SCREEN_ROWS)
        return;
    target = SCREEN_BITMAP + robots_screen_cell_offset(x, y);
#ifdef ROBOTS_TIMEX_HIRES
    if ((x & 1u) != 0u)
        target += SCREEN_PLANE_STRIDE;
    for (row = 0u; row < 8u; ++row) {
        *target = glyph[row];
        target += ROBOTS_SCREEN_ROW_STRIDE;
    }
#else
    /*
     * Two 4x8 characters share one display byte, so each row is a
     * read-modify-write.  Which nibble to keep is decided once, outside the
     * loop, rather than on all eight rows.
     */
    if ((x & 1u) == 0u) {
        for (row = 0u; row < 8u; ++row) {
            *target = (unsigned char)((*target & 0x0fu) | (glyph[row] & 0xf0u));
            target += ROBOTS_SCREEN_ROW_STRIDE;
        }
    } else {
        for (row = 0u; row < 8u; ++row) {
            *target = (unsigned char)((*target & 0xf0u) | (glyph[row] & 0x0fu));
            target += ROBOTS_SCREEN_ROW_STRIDE;
        }
    }
#endif
}

static void screen_put(unsigned char x, unsigned char y, unsigned char value)
{
    const unsigned char *glyph;

#ifdef ROBOTS_TIMEX_HIRES
    if (value < ROBOTS_FONT8X8_FIRST || value >= 128u)
        value = ' ';
    glyph = &robots_font8x8[
        (unsigned int)(value - ROBOTS_FONT8X8_FIRST) * 8u];
#else
    if (value < ROBOTS_FONT_FIRST || value >= 128u)
        value = ' ';
    glyph = &robots_font4x8[(unsigned int)(value - ROBOTS_FONT_FIRST) * 8u];
#endif
    screen_put_glyph(x, y, glyph);
}

static void screen_put_robot(unsigned char x, unsigned char y)
{
#ifdef ROBOTS_TIMEX_HIRES
    const uint8_t *glyph;

    glyph = robots_timex_theme_enemy(timex_theme);
    if (glyph == NULL)
        screen_put(x, y, '+');
    else
        screen_put_glyph(x, y, glyph);
#else
    screen_put_glyph(x, y, robots_robot4x8);
#endif
}

static void screen_put_object(unsigned char x, unsigned char y,
                              unsigned char value)
{
    if (value == '+')
        screen_put_robot(x, y);
    else
        screen_put(x, y, value);
}

static void screen_text(unsigned char x, unsigned char y, const char *text)
{
    while (*text != '\0' && x < SCREEN_COLUMNS) {
        screen_put(x, y, (unsigned char)*text);
        ++x;
        ++text;
    }
}

static unsigned char text_length(const char *text)
{
    unsigned char length;

    length = 0u;
    while (*text != '\0') {
        ++length;
        ++text;
    }
    return length;
}

static void screen_center(unsigned char y, const char *text)
{
    unsigned char length;

    length = text_length(text);
    screen_text((unsigned char)((SCREEN_COLUMNS - length) >> 1), y, text);
}

static void screen_number(unsigned char x, unsigned char y,
                          unsigned long value, unsigned char width)
{
    unsigned char digits[10];
    unsigned char i;

    if (width > sizeof(digits))
        width = sizeof(digits);
    for (i = 0u; i < width; ++i) {
        digits[(unsigned char)(width - i - 1u)] =
            (unsigned char)('0' + (unsigned char)(value % 10ul));
        value /= 10ul;
    }
    for (i = 0u; i < width; ++i)
        screen_put((unsigned char)(x + i), y, digits[i]);
}

#ifndef ROBOTS_TIMEX_HIRES
static void fill_attributes(unsigned char attribute)
{
    memset(SCREEN_ATTRIBUTES, attribute, SCREEN_ATTRIBUTE_BYTES);
    /* Whatever cell was red has just been overwritten along with the rest. */
    player_attribute = NO_ATTRIBUTE;
}

/*
 * Only one attribute cell is ever special, so repainting all 768 of them to
 * move the player's red highlight was pure waste: clear the old cell, set the
 * new one, and skip the work entirely when the player has not changed cells.
 */
static void move_player_colour(void)
{
    unsigned int offset;

    offset = robots_screen_attribute_offset(
        (unsigned char)((unsigned char)game.player.x + FIELD_LEFT),
        (unsigned char)((unsigned char)game.player.y + 1u));
    if (offset == player_attribute)
        return;
    if (player_attribute != NO_ATTRIBUTE)
        SCREEN_ATTRIBUTES[player_attribute] = ATTR(ZX_WHITE, ZX_BLACK);
    SCREEN_ATTRIBUTES[offset] = ATTR(ZX_RED, ZX_BLACK);
    player_attribute = offset;
}
#endif

static void clear_screen(void)
{
    memset(SCREEN_BITMAP, 0, SCREEN_BITMAP_BYTES);
#ifdef ROBOTS_TIMEX_HIRES
    memset(SCREEN_BITMAP + SCREEN_PLANE_STRIDE, 0, SCREEN_BITMAP_BYTES);
#else
    fill_attributes(ATTR(ZX_WHITE, ZX_BLACK));
#endif
}

static void blank_live_screen(void)
{
    clear_screen();
#ifdef ROBOTS_TIMEX_HIRES
    z80_outp(TIMEX_SCLD_PORT, TIMEX_HIRES_WHITE_ON_BLACK);
#endif
}

#ifdef ROBOTS_TIMEX_HIRES
static void restore_timex_ula(void)
{
    /* Prepare a valid 256x192 ULA screen before changing display modes. */
    memset(SCREEN_BITMAP, 0, SCREEN_BITMAP_BYTES);
    memset(TIMEX_ULA_ATTRIBUTES, TIMEX_ULA_WHITE_ON_BLACK,
           SCREEN_ATTRIBUTE_BYTES);
    z80_outp(TIMEX_SCLD_PORT, TIMEX_ULA_MODE);
}
#endif

/*
 * Debouncing happens before the wait, not after it.  Waiting for the key to
 * come back up first meant nothing was drawn until the player let go, which
 * read as lag on top of the redraw; this way the board updates on the press
 * and the release is absorbed while the finger is already lifting.  One press
 * still means one turn.
 */
static int read_key(void)
{
    int key;

    in_wait_nokey();
    do {
        key = in_inkey();
        if (key == 0)
            z80_delay_ms(5u);
    } while (key == 0);
    return key;
}

static const char *license_page_1[24] = {
    "COPYRIGHT (C) 1980, 1993",
    "    THE REGENTS OF THE UNIVERSITY OF CALIFORNIA.  ALL",
    "RIGHTS RESERVED.",
    "COPYRIGHT (C) 2026 MICHAL PASTERNAK.",
    "    ALL RIGHTS RESERVED.",
    "",
    "REDISTRIBUTION AND USE IN SOURCE AND BINARY FORMS, WITH OR",
    "WITHOUT MODIFICATION, ARE PERMITTED PROVIDED THAT THE",
    "FOLLOWING CONDITIONS ARE MET:",
    "",
    "1. REDISTRIBUTIONS OF SOURCE CODE MUST RETAIN THE ABOVE",
    "   COPYRIGHT NOTICE, THIS LIST OF CONDITIONS AND THE",
    "   FOLLOWING DISCLAIMER.",
    "2. REDISTRIBUTIONS IN BINARY FORM MUST REPRODUCE THE ABOVE",
    "   COPYRIGHT NOTICE, THIS LIST OF CONDITIONS AND THE",
    "   FOLLOWING DISCLAIMER IN THE DOCUMENTATION AND/OR OTHER",
    "   MATERIALS PROVIDED WITH THE DISTRIBUTION.",
    "", "", "", "", "", "",
    "SPACE: NEXT  Q: CLOSE                         BSD LICENSE 1/2"
};

static const char *license_page_2[24] = {
    "3. NEITHER THE NAME OF THE UNIVERSITY NOR THE NAMES OF ITS",
    "   CONTRIBUTORS MAY BE USED TO ENDORSE OR PROMOTE PRODUCTS",
    "   DERIVED FROM THIS SOFTWARE WITHOUT SPECIFIC PRIOR WRITTEN",
    "   PERMISSION.",
    "",
    "THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS",
    "``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,",
    "BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY",
    "AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO",
    "EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY",
    "DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR",
    "CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,",
    "PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,",
    "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED",
    "AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT",
    "LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)",
    "ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF",
    "ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.",
    "", "", "", "", "",
    "SPACE: CLOSE  P: PREV                        BSD LICENSE 2/2"
};

static void show_title(void);

static void show_license_page(const char **page)
{
    unsigned char y;

    clear_screen();
    for (y = 0u; y < SCREEN_ROWS; ++y)
        screen_text(1u, y, page[y]);
}

static void show_license(void)
{
    unsigned char page;
    int key;

    page = 0u;
    for (;;) {
        if (page == 0u)
            show_license_page(license_page_1);
        else
            show_license_page(license_page_2);

        key = read_key();
        if (key == 'q' || key == 'Q')
            return;
        if (page == 0u && (key == ' ' || key == 13))
            page = 1u;
        else if (page != 0u && (key == 'p' || key == 'P'))
            page = 0u;
        else if (page != 0u && (key == ' ' || key == 13))
            return;
    }
}

static unsigned int title_seed(void)
{
    unsigned int seed;
    int key;

    seed = 0xACE1u;
    in_wait_nokey();
    for (;;) {
        key = 0;
        while (key == 0) {
            if ((seed & 1u) != 0u)
                seed = (unsigned int)((seed >> 1) ^ 0xB400u);
            else
                seed >>= 1;
            key = in_inkey();
        }
        in_wait_nokey();

        if (key == 'l' || key == 'L') {
            show_license();
            show_title();
#ifdef ROBOTS_TIMEX_HIRES
        } else if (key == 'g' || key == 'G') {
            ++timex_theme;
            if (timex_theme >= ROBOTS_TIMEX_THEME_COUNT)
                timex_theme = ROBOTS_TIMEX_THEME_ORIGINAL;
            show_title();
#endif
        } else {
            seed ^= (unsigned int)((unsigned int)key << 8);
            if (seed == 0u)
                seed = 1u;
            return seed;
        }
    }
}

static void show_title(void)
{
    unsigned char x;
    unsigned char y;
#ifdef ROBOTS_TIMEX_HIRES
    unsigned char label_end;
    const char *theme_label;
#endif

    clear_screen();

    screen_center(0u, "ZX BSD ROBOTS");
#ifdef ROBOTS_TIMEX_HIRES
    screen_center(1u, "THE CLASSIC BSD GAME - TIMEX HI-RES 512X192");
#else
    screen_center(1u, "THE CLASSIC BSD GAME - ZX SPECTRUM 48K");
#endif
    screen_center(3u, "{--------------------------------------}");
    for (y = 4u; y <= 7u; ++y) {
        screen_put(12u, y, '|');
        screen_put(51u, y, '|');
    }
    for (x = 18u; x <= 45u; x = (unsigned char)(x + 9u))
        screen_put_robot(x, 4u);
    screen_put_robot(23u, 5u);
    screen_put_robot(41u, 5u);
    screen_put(32u, 5u, '@');
    screen_put(20u, 7u, '*');
    screen_put(28u, 7u, '*');
    screen_put(36u, 7u, '*');
    screen_put(44u, 7u, '*');
    screen_center(8u, "[--------------------------------------]");
    screen_center(9u, "NO WEAPONS. MAKE THE ROBOTS COLLIDE.");

    screen_center(11u, "MOVE: ORIGINAL KEYS       NUMBER KEYS");
    screen_center(12u, "Y K U               7 8 9");
    screen_center(13u, "H . L      OR       4 5 6");
    screen_center(14u, "B J N               1 2 3");
    screen_center(16u, "T TELEPORT   W RISKY WAIT   S/> SAFE WAIT");
    screen_center(17u, "CAPS+MOVE RUNS SAFELY   I HELP   Q QUIT");
#ifdef ROBOTS_TIMEX_HIRES
    theme_label = robots_timex_theme_label(timex_theme);
    screen_text(18u, 18u, "G: THEME [");
    screen_text(28u, 18u, theme_label);
    label_end = (unsigned char)(28u + text_length(theme_label));
    screen_put(label_end, 18u, ']');
#else
    screen_center(18u, "KEYBOARD CONTROLS ONLY");
#endif

    screen_text(18u, 19u, "ENEMY");
    screen_put_robot(24u, 19u);
    screen_text(29u, 19u, "YOU @   HEAP *");
    screen_center(20u, "L: BSD LICENSE");
    screen_center(21u, "PRESS ANY OTHER KEY TO PLAY");
    screen_center(22u, "ORIGINAL GAME BY KEN ARNOLD, 1980");
    board_visible = 0u;
}

static void show_help(void)
{
    int key;

    clear_screen();
    screen_center(0u, "HOW TO PLAY");
    screen_text(2u, 2u, "YOU ARE @. ROBOTS ARE  . JUNK HEAPS ARE *.");
    screen_put_robot(24u, 2u);
    screen_text(2u, 3u, "EVERY ROBOT TAKES ONE STEP TOWARD @ AFTER YOUR TURN.");
    screen_text(2u, 4u, "LURE THEM INTO EACH OTHER OR INTO A HEAP.");
    screen_text(2u, 5u, "A NORMAL MOVE THAT WOULD KILL YOU IS REJECTED.");

    screen_text(8u, 7u, "Y K U             7 8 9");
    screen_text(8u, 8u, " \\|/               \\|/");
    screen_text(8u, 9u, "H . L      OR     4 5 6");
    screen_text(8u, 10u, " /|\\               /|\\");
    screen_text(8u, 11u, "B J N             1 2 3");

    screen_text(2u, 13u, "SPACE / . / 5    STAND STILL FOR ONE SAFE TURN");
    screen_text(2u, 14u, "CAPS + DIRECTION RUN SAFELY IN THAT DIRECTION");
    screen_text(2u, 15u, "S OR >           STAND SAFELY AS LONG AS POSSIBLE");
    screen_text(2u, 16u, "T                TELEPORT TO A RANDOM EMPTY CELL");
    screen_text(2u, 17u, "W                RISKY WAIT UNTIL THE FIELD ENDS");
    screen_text(2u, 18u, "I                THIS HELP SCREEN");
    screen_text(2u, 19u, "Q                QUIT");
    screen_text(2u, 20u, "ALL CONTROLS USE THE SPECTRUM KEYBOARD");
    screen_text(2u, 21u, "W BONUS: +1 FOR EACH ROBOT KILLED, ONLY IF YOU LIVE.");
    screen_center(22u, "PRESS SPACE TO RETURN");
    do {
        key = read_key();
    } while (key != ' ' && key != 13);
    board_visible = 0u;
}

/*
 * The play field is one flat array, not [y][x].  Its width of 59 is not a
 * power of two, so every two-dimensional subscript made sdcc synthesise a
 * multiply; at 1298 cells scanned twice per redraw that alone cost more than
 * a tenth of a second.  Indices are computed here, for the few dozen objects
 * that exist, and the scan below walks with plain pointers.
 */
static unsigned int cell_index(signed char x, signed char y)
{
    return (unsigned int)((unsigned char)y * ROBOTS_GAME_WIDTH) +
           (unsigned char)x;
}

static void build_render_cells(void)
{
    unsigned char i;

    memset(render_cells, ' ', sizeof(render_cells));
    for (i = 0u; i < game.heap_count; ++i)
        render_cells[cell_index(game.heaps[i].x, game.heaps[i].y)] = '*';
    for (i = 0u; i < game.robot_count; ++i)
        render_cells[cell_index(game.robots[i].x, game.robots[i].y)] = '+';
    render_cells[cell_index(game.player.x, game.player.y)] = '@';
}

/* Status fields already on screen, so an unchanged one costs nothing. */
static unsigned int shown_level;
static unsigned long shown_score;
static unsigned long shown_high_score;
static unsigned char shown_robots;
static unsigned char status_drawn;

static void draw_frame(void)
{
    unsigned char x;
    unsigned char y;

    clear_screen();
    for (x = 0u; x <= FIELD_RIGHT; ++x) {
        screen_put(x, 0u, '-');
        screen_put(x, 23u, '-');
    }
    screen_put(0u, 0u, '{');
    screen_put(FIELD_RIGHT, 0u, '}');
    screen_put(0u, 23u, '[');
    screen_put(FIELD_RIGHT, 23u, ']');
    for (y = 1u; y < 23u; ++y) {
        screen_put(0u, y, '|');
        screen_put(FIELD_RIGHT, y, '|');
    }

    /* The labels never change, so they belong with the frame rather than in
       draw_status(), which used to repaint them on every single redraw. */
    screen_text(2u, 0u, "ROBOTS");
    screen_text(9u, 0u, "L");
    screen_text(14u, 0u, "S");
    screen_text(22u, 0u, "HI");
    screen_text(31u, 0u, "R");
    screen_text(36u, 0u, "I:HELP");
    screen_text(43u, 0u, "T:TELE");
    screen_text(50u, 0u, "W:WAIT");

    /*
     * clear_screen() has just blanked the display, and a blank play field is
     * one of spaces -- so record it as such.  Marking every cell unknown
     * instead made the first redraw of a level plot all 1298 cells, some 1200
     * of them spaces onto already-empty ground.
     */
    memset(previous_cells, ' ', sizeof(previous_cells));
    status_drawn = 0u;
}

static void draw_status(void)
{
    if (status_drawn == 0u || shown_level != game.level) {
        screen_number(10u, 0u, (unsigned long)game.level, 3u);
        shown_level = game.level;
    }
    if (status_drawn == 0u || shown_score != game.score) {
        screen_number(15u, 0u, game.score, 6u);
        shown_score = game.score;
    }
    if (status_drawn == 0u || shown_high_score != high_score) {
        screen_number(24u, 0u, high_score, 6u);
        shown_high_score = high_score;
    }
    if (status_drawn == 0u || shown_robots != game.robot_count) {
        screen_number(32u, 0u, (unsigned long)game.robot_count, 2u);
        shown_robots = game.robot_count;
    }
    /* Clears the ! or ? notice left by the previous turn. */
    screen_text(61u, 0u, "   ");
    status_drawn = 1u;
}

static void render_board(void)
{
    const unsigned char *current;
    unsigned char *previous;
    unsigned char x;
    unsigned char y;

    if (game.score > high_score)
        high_score = game.score;
    build_render_cells();
    if (board_visible == 0u) {
        draw_frame();
        board_visible = 1u;
    }
    draw_status();

    current = render_cells;
    previous = previous_cells;
    for (y = 0u; y < ROBOTS_GAME_HEIGHT; ++y) {
        for (x = 0u; x < ROBOTS_GAME_WIDTH; ++x) {
            if (*previous != *current) {
                *previous = *current;
                screen_put_object((unsigned char)(x + FIELD_LEFT),
                                  (unsigned char)(y + 1u), *current);
            }
            ++previous;
            ++current;
        }
    }
#ifndef ROBOTS_TIMEX_HIRES
    move_player_colour();
#endif
}

static void show_notice(unsigned char value)
{
    screen_put(NOTICE_COLUMN, 0u, value);
}

static void modal_box(const char *title, const char *line1,
                      const char *line2, const char *line3)
{
    unsigned char x;
    unsigned char y;

#ifndef ROBOTS_TIMEX_HIRES
    fill_attributes(ATTR(ZX_WHITE, ZX_BLACK));
#endif
    for (y = 7u; y <= 16u; ++y) {
        screen_put(9u, y, '|');
        for (x = 10u; x < 54u; ++x)
            screen_put(x, y, ' ');
        screen_put(54u, y, '|');
    }
    for (x = 9u; x <= 54u; ++x) {
        screen_put(x, 7u, '-');
        screen_put(x, 16u, '-');
    }
    screen_put(9u, 7u, '{');
    screen_put(54u, 7u, '}');
    screen_put(9u, 16u, '[');
    screen_put(54u, 16u, ']');
    screen_text(12u, 9u, title);
    screen_text(12u, 11u, line1);
    screen_text(12u, 12u, line2);
    screen_text(12u, 14u, line3);
}

static unsigned char confirm_quit(void)
{
    int key;

    modal_box("QUIT GAME?", "Y  RETURN TO BASIC", "N  KEEP PLAYING",
              "PRESS Y OR N");
    do {
        key = read_key();
    } while (key != 'y' && key != 'Y' && key != 'n' && key != 'N');
    if (key == 'y' || key == 'Y')
        return 1u;
    board_visible = 0u;
    render_board();
    return 0u;
}

static void show_level_clear(void)
{
    render_board();
    if (game.score > high_score)
        high_score = game.score;
    if (game.last_wait_bonus != 0u)
        modal_box("FIELD CLEARED!", "WAIT BONUS AWARDED",
                  "+1 PER ROBOT", "PRESS ANY KEY");
    else
        modal_box("FIELD CLEARED!", "ALL ROBOTS DESTROYED",
                  "NEXT FIELD IS HARDER", "PRESS ANY KEY");
    screen_text(34u, 11u, "SCORE");
    screen_number(40u, 11u, game.score, 6u);
    if (game.last_wait_bonus != 0u) {
        screen_text(34u, 12u, "BONUS");
        screen_number(40u, 12u,
                      (unsigned long)game.last_wait_bonus, 2u);
    }
    read_key();
    robots_game_next_level(&game);
    board_visible = 0u;
}

static unsigned char show_game_over(void)
{
    int key;

    render_board();
    if (game.score > high_score)
        high_score = game.score;
    modal_box("AARRRRGGHHH... GAME OVER", "FINAL SCORE",
              "R  PLAY AGAIN", "Q  RETURN TO BASIC");
    screen_number(29u, 11u, game.score, 6u);
    do {
        key = read_key();
    } while (key != 'r' && key != 'R' && key != 'q' && key != 'Q');
    return (unsigned char)(key == 'q' || key == 'Q');
}

static unsigned char run_safely(signed char dx, signed char dy)
{
    unsigned char result;

    result = robots_game_move(&game, dx, dy);
    if (result == ROBOTS_GAME_RESULT_REJECTED) {
        show_notice('!');
        return result;
    }
    while (result == ROBOTS_GAME_RESULT_RUNNING) {
        render_board();
        z80_delay_ms(ANIMATION_DELAY_MS);
        result = robots_game_move(&game, dx, dy);
    }
    if (result != ROBOTS_GAME_RESULT_REJECTED)
        render_board();
    return result;
}

static unsigned char wait_to_end(void)
{
    unsigned char result;

    result = robots_game_wait_begin(&game);
    if (result == ROBOTS_GAME_RESULT_REJECTED) {
        show_notice('!');
        return result;
    }
    do {
        result = robots_game_wait_step(&game);
        render_board();
        z80_delay_ms(WAIT_DELAY_MS);
    } while (result == ROBOTS_GAME_RESULT_RUNNING);
    return result;
}

static unsigned char play_game(void)
{
    int key;
    signed char dx;
    signed char dy;
    unsigned char run;
    unsigned char result;
    RobotsControlAction action;

    /*
     * The board is drawn once per turn, by whichever branch changed it.  It
     * used to be redrawn at the top of this loop as well, so every keypress
     * paid for two full redraws where one was enough.
     */
    render_board();
    for (;;) {
        key = read_key();
        action = robots_control_decode(key, &dx, &dy, &run);
        result = ROBOTS_GAME_RESULT_REJECTED;

        if (action == ROBOTS_CONTROL_HELP) {
            show_help();
            render_board();
            continue;
        }
        if (action == ROBOTS_CONTROL_QUIT) {
            if (confirm_quit() != 0u)
                return 1u;
            continue; /* confirm_quit() has already redrawn the board */
        }
        if (action == ROBOTS_CONTROL_TELEPORT) {
            result = robots_game_teleport(&game);
            if (result != ROBOTS_GAME_RESULT_REJECTED)
                render_board();
        } else if (action == ROBOTS_CONTROL_WAIT) {
            result = wait_to_end();
        } else if (action == ROBOTS_CONTROL_MOVE) {
            if (run != 0u)
                result = run_safely(dx, dy);
            else {
                result = robots_game_move(&game, dx, dy);
                if (result == ROBOTS_GAME_RESULT_REJECTED)
                    show_notice('!');
                else
                    render_board();
            }
        } else {
            show_notice('?');
        }

        if (result == ROBOTS_GAME_RESULT_CLEARED) {
            show_level_clear();
            render_board(); /* the modal invalidated the board; rebuild it */
        } else if (result == ROBOTS_GAME_RESULT_DEAD)
            return show_game_over();
    }
}

int main(void)
{
    unsigned int seed;

    z80_outp(0xfeu, 0u);
    blank_live_screen();
    high_score = 0ul;
#ifdef ROBOTS_TIMEX_HIRES
    timex_theme = ROBOTS_TIMEX_THEME_ORIGINAL;
#endif
    for (;;) {
        show_title();
        seed = title_seed();
        robots_game_init(&game, seed);
        board_visible = 0u;
        if (play_game() != 0u)
            break;
    }

#ifdef ROBOTS_TIMEX_HIRES
    restore_timex_ula();
#else
    clear_screen();
    screen_center(10u, "THANKS FOR PLAYING ROBOTS");
    screen_center(12u, "RANDOMIZE USR TO PLAY AGAIN");
#endif
    return 0;
}
