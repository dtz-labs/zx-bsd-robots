#include <input.h>
#include <string.h>
#include <z80.h>

#include "font4x8.h"
#include "glyph_styles.h"
#include "robots_game.h"

#define SCREEN_COLUMNS 64u
#define SCREEN_ROWS 24u
#define FIELD_LEFT 1u
#define FIELD_RIGHT 60u
#define NOTICE_COLUMN 62u

#define SCREEN_BITMAP_BYTES 6144u
#define SCREEN_ATTRIBUTE_BYTES 768u
#define SCREEN_BYTES (SCREEN_BITMAP_BYTES + SCREEN_ATTRIBUTE_BYTES)
#define ZX_BITMAP_BASE ((void *)16384u)
#define ZX_ATTRIBUTE_BASE ((void *)22528u)
#define ZX_BLACK 0u
#define ZX_WHITE 7u
#define ZX_BRIGHT 64u

#define ATTR(ink_, paper_) \
    ((unsigned char)(ZX_BRIGHT | (ink_) | ((paper_) << 3)))

#define ANIMATION_DELAY_MS 90u
#define WAIT_DELAY_MS 150u

static RobotsGame game;
static unsigned long high_score;

/*
 * The complete 6912-byte display is composed off-screen.  Attributes are
 * copied first and the bitmap second, avoiding progressive redraw and the
 * CRT terminal's temporary black-ink-on-white-paper attributes.  All large
 * buffers are global so they cannot exhaust the Z80 stack.
 */
static unsigned char screen_buffer[SCREEN_BYTES];
static unsigned char render_cells[ROBOTS_GAME_HEIGHT][ROBOTS_GAME_WIDTH];
static unsigned char previous_cells[ROBOTS_GAME_HEIGHT][ROBOTS_GAME_WIDTH];
static unsigned char board_visible;
static unsigned char glyph_style;

static unsigned int bitmap_offset(unsigned char x_byte,
                                  unsigned char pixel_y)
{
    return (unsigned int)(((unsigned int)(pixel_y & 0xc0u) << 5) |
                          ((unsigned int)(pixel_y & 0x07u) << 8) |
                          ((unsigned int)(pixel_y & 0x38u) << 2) |
                          x_byte);
}

static void screen_put_glyph(unsigned char x, unsigned char y,
                             const unsigned char *glyph)
{
    unsigned int offset;
    unsigned char pixel_y;
    unsigned char row;
    unsigned char bits;

    if (x >= SCREEN_COLUMNS || y >= SCREEN_ROWS)
        return;
    pixel_y = (unsigned char)(y * 8u);
    for (row = 0u; row < 8u; ++row) {
        offset = bitmap_offset((unsigned char)(x >> 1),
                               (unsigned char)(pixel_y + row));
        bits = glyph[row];
        if ((x & 1u) == 0u)
            screen_buffer[offset] =
                (unsigned char)((screen_buffer[offset] & 0x0fu) |
                                (bits & 0xf0u));
        else
            screen_buffer[offset] =
                (unsigned char)((screen_buffer[offset] & 0xf0u) |
                                (bits & 0x0fu));
    }
}

static void screen_put(unsigned char x, unsigned char y, unsigned char value)
{
    const unsigned char *glyph;

    if (value < ROBOTS_FONT_FIRST || value >= 128u)
        value = ' ';
    glyph = &robots_font4x8[(unsigned int)(value - ROBOTS_FONT_FIRST) * 8u];
    screen_put_glyph(x, y, glyph);
}

static void screen_put_robot(unsigned char x, unsigned char y)
{
    screen_put_glyph(x, y, robots_glyph_style_get(glyph_style)->robot);
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

static void fill_attributes(unsigned char attribute)
{
    unsigned int i;

    for (i = 0u; i < SCREEN_ATTRIBUTE_BYTES; ++i)
        screen_buffer[SCREEN_BITMAP_BYTES + i] = attribute;
}

static void clear_screen(void)
{
    memset(screen_buffer, 0, SCREEN_BITMAP_BYTES);
    fill_attributes(ATTR(ZX_WHITE, ZX_BLACK));
}

static void present_screen(void)
{
    /* Establish black paper before exposing the newly composed bitmap. */
    memcpy(ZX_ATTRIBUTE_BASE, &screen_buffer[SCREEN_BITMAP_BYTES],
           SCREEN_ATTRIBUTE_BYTES);
    memcpy(ZX_BITMAP_BASE, screen_buffer, SCREEN_BITMAP_BYTES);
}

static int read_key(void)
{
    int key;

    do {
        key = in_inkey();
        if (key == 0)
            z80_delay_ms(5u);
    } while (key == 0);
    in_wait_nokey();
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
    present_screen();
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
        } else if (key == 'g' || key == 'G') {
            glyph_style = (unsigned char)(glyph_style + 1u);
            if (glyph_style >= ROBOTS_GLYPH_STYLE_COUNT)
                glyph_style = ROBOTS_GLYPH_STYLE_BSD;
            show_title();
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
    const RobotsGlyphStyle *style;
    unsigned char x;
    unsigned char y;
    unsigned char label_end;

    clear_screen();
    style = robots_glyph_style_get(glyph_style);

    screen_center(0u, "R O B O T S");
    screen_center(1u, "THE CLASSIC BSD GAME - ZX SPECTRUM 48K");
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
    screen_center(16u, "T/0 TELEPORT   W RISKY WAIT   S/> SAFE WAIT");
    screen_center(17u, "CAPS+MOVE RUNS SAFELY   I HELP   Q QUIT");

    screen_text(4u, 19u, "G: GLYPHS [");
    screen_text(15u, 19u, style->label);
    label_end = (unsigned char)(15u + text_length(style->label));
    screen_put(label_end, 19u, ']');
    screen_text(23u, 19u, "ENEMY");
    screen_put_robot(29u, 19u);
    screen_text(33u, 19u, "YOU @   HEAP *");
    screen_center(20u, "L: BSD LICENSE");
    screen_center(21u, "PRESS ANY OTHER KEY TO PLAY");
    screen_center(22u, "ORIGINAL GAME BY KEN ARNOLD");
    board_visible = 0u;
    present_screen();
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
    screen_text(2u, 16u, "T OR 0           TELEPORT TO A RANDOM EMPTY CELL");
    screen_text(2u, 17u, "W                RISKY WAIT UNTIL THE FIELD ENDS");
    screen_text(2u, 18u, "I                THIS HELP SCREEN");
    screen_text(2u, 19u, "Q                QUIT");
    screen_text(2u, 21u, "W BONUS: +1 FOR EACH ROBOT KILLED, ONLY IF YOU LIVE.");
    screen_center(22u, "PRESS SPACE TO RETURN");
    present_screen();
    do {
        key = read_key();
    } while (key != ' ' && key != 13);
    board_visible = 0u;
}

static void build_render_cells(void)
{
    unsigned char x;
    unsigned char y;
    unsigned char i;

    for (y = 0u; y < ROBOTS_GAME_HEIGHT; ++y)
        for (x = 0u; x < ROBOTS_GAME_WIDTH; ++x)
            render_cells[y][x] = ' ';

    for (i = 0u; i < game.heap_count; ++i)
        render_cells[(unsigned char)game.heaps[i].y]
                    [(unsigned char)game.heaps[i].x] = '*';
    for (i = 0u; i < game.robot_count; ++i)
        render_cells[(unsigned char)game.robots[i].y]
                    [(unsigned char)game.robots[i].x] = '+';
    render_cells[(unsigned char)game.player.y]
                [(unsigned char)game.player.x] = '@';
}

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
    memset(previous_cells, 0xff, sizeof(previous_cells));
}

static void draw_status(void)
{
    unsigned char x;

    for (x = 1u; x < FIELD_RIGHT; ++x)
        screen_put(x, 0u, '-');
    screen_put(0u, 0u, '{');
    screen_put(FIELD_RIGHT, 0u, '}');
    screen_text(2u, 0u, "ROBOTS");
    screen_text(9u, 0u, "L");
    screen_number(10u, 0u, (unsigned long)game.level, 3u);
    screen_text(14u, 0u, "S");
    screen_number(15u, 0u, game.score, 6u);
    screen_text(22u, 0u, "HI");
    screen_number(24u, 0u, high_score, 6u);
    screen_text(31u, 0u, "R");
    screen_number(32u, 0u, (unsigned long)game.robot_count, 2u);
    screen_text(36u, 0u, "I:HELP");
    screen_text(43u, 0u, "T:TELE");
    screen_text(50u, 0u, "W:WAIT");
    screen_text(61u, 0u, "   ");
}

static void render_board(void)
{
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

    for (y = 0u; y < ROBOTS_GAME_HEIGHT; ++y) {
        for (x = 0u; x < ROBOTS_GAME_WIDTH; ++x) {
            if (previous_cells[y][x] != render_cells[y][x]) {
                screen_put_object((unsigned char)(x + FIELD_LEFT),
                                  (unsigned char)(y + 1u),
                                  render_cells[y][x]);
                previous_cells[y][x] = render_cells[y][x];
            }
        }
    }
    present_screen();
}

static void show_notice(unsigned char value)
{
    screen_put(NOTICE_COLUMN, 0u, value);
    present_screen();
}

static void modal_box(const char *title, const char *line1,
                      const char *line2, const char *line3)
{
    unsigned char x;
    unsigned char y;

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
    present_screen();
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
    present_screen();
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
    present_screen();
    do {
        key = read_key();
    } while (key != 'r' && key != 'R' && key != 'q' && key != 'Q');
    return (unsigned char)(key == 'q' || key == 'Q');
}

static unsigned char direction_for_key(int key, signed char *dx,
                                       signed char *dy, unsigned char *run)
{
    *dx = 0;
    *dy = 0;
    *run = 0u;

    switch (key) {
    case 'y': case '7': *dx = -1; *dy = -1; return 1u;
    case 'k': case '8': *dy = -1; return 1u;
    case 'u': case '9': *dx = 1; *dy = -1; return 1u;
    case 'h': case '4': *dx = -1; return 1u;
    case 'l': case '6': *dx = 1; return 1u;
    case 'b': case '1': *dx = -1; *dy = 1; return 1u;
    case 'j': case '2': *dy = 1; return 1u;
    case 'n': case '3': *dx = 1; *dy = 1; return 1u;
    case ' ': case '.': case '5': case 13: return 1u;
    case 'Y': *dx = -1; *dy = -1; *run = 1u; return 1u;
    case 'K': *dy = -1; *run = 1u; return 1u;
    case 'U': *dx = 1; *dy = -1; *run = 1u; return 1u;
    case 'H': *dx = -1; *run = 1u; return 1u;
    case 'L': *dx = 1; *run = 1u; return 1u;
    case 'B': *dx = -1; *dy = 1; *run = 1u; return 1u;
    case 'J': *dy = 1; *run = 1u; return 1u;
    case 'N': *dx = 1; *dy = 1; *run = 1u; return 1u;
    case 's': case 'S': case '>': *run = 1u; return 1u;
    default: return 0u;
    }
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

    for (;;) {
        render_board();
        key = read_key();
        result = ROBOTS_GAME_RESULT_REJECTED;

        if (key == 'i' || key == 'I' || key == '?') {
            show_help();
            continue;
        }
        if (key == 'q' || key == 'Q') {
            if (confirm_quit() != 0u)
                return 1u;
            continue;
        }
        if (key == 't' || key == 'T' || key == '0') {
            result = robots_game_teleport(&game);
            if (result != ROBOTS_GAME_RESULT_REJECTED)
                render_board();
        } else if (key == 'w' || key == 'W') {
            result = wait_to_end();
        } else if (direction_for_key(key, &dx, &dy, &run) != 0u) {
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

        if (result == ROBOTS_GAME_RESULT_CLEARED)
            show_level_clear();
        else if (result == ROBOTS_GAME_RESULT_DEAD)
            return show_game_over();
    }
}

int main(void)
{
    unsigned int seed;

    z80_outp(0xfeu, ZX_BLACK);
    high_score = 0ul;
    glyph_style = ROBOTS_GLYPH_STYLE_ROBOT;

    for (;;) {
        show_title();
        seed = title_seed();
        robots_game_init(&game, seed);
        board_visible = 0u;
        if (play_game() != 0u)
            break;
    }

    clear_screen();
    screen_center(10u, "THANKS FOR PLAYING ROBOTS");
    screen_center(12u, "RANDOMIZE USR TO PLAY AGAIN");
    present_screen();
    return 0;
}
