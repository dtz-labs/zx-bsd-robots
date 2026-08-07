#include <input.h>
#include <stdio.h>
#include <string.h>
#include <z80.h>

#include "robots_game.h"

#define SCREEN_COLUMNS 64u
#define SCREEN_ROWS 24u
#define FIELD_LEFT 1u
#define FIELD_RIGHT 60u
#define NOTICE_COLUMN 62u

#define ZX_ATTR_BASE ((volatile unsigned char *)22528u)
#define ZX_BLACK 0u
#define ZX_BLUE 1u
#define ZX_RED 2u
#define ZX_MAGENTA 3u
#define ZX_GREEN 4u
#define ZX_CYAN 5u
#define ZX_YELLOW 6u
#define ZX_WHITE 7u
#define ZX_BRIGHT 64u

#define ATTR(ink_, paper_) \
    ((unsigned char)(ZX_BRIGHT | (ink_) | ((paper_) << 3)))

#define ANIMATION_DELAY_MS 90u
#define WAIT_DELAY_MS 150u

static RobotsGame game;
static unsigned long high_score;

/* Global buffers keep the 2.6 KB play field off the small Z80 stack. */
static unsigned char render_cells[ROBOTS_GAME_HEIGHT][ROBOTS_GAME_WIDTH];
static unsigned char previous_cells[ROBOTS_GAME_HEIGHT][ROBOTS_GAME_WIDTH];
static unsigned char board_visible;

static void screen_at(unsigned char x, unsigned char y)
{
    putchar(22);
    putchar((int)(x + 1u));
    putchar((int)(y + 1u));
}

static void screen_put(unsigned char x, unsigned char y, unsigned char value)
{
    screen_at(x, y);
    putchar((int)value);
}

static void screen_text(unsigned char x, unsigned char y, const char *text)
{
    screen_at(x, y);
    while (*text != '\0') {
        putchar((unsigned char)*text);
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
    screen_at(x, y);
    for (i = 0u; i < width; ++i)
        putchar(digits[i]);
}

static void fill_attributes(unsigned char attribute)
{
    unsigned int i;

    for (i = 0u; i < 768u; ++i)
        ZX_ATTR_BASE[i] = attribute;
}

static void set_attribute_span(unsigned char y, unsigned char x0,
                               unsigned char x1, unsigned char attribute)
{
    unsigned char first;
    unsigned char last;
    unsigned char x;
    unsigned int offset;

    first = (unsigned char)(x0 >> 1);
    last = (unsigned char)(x1 >> 1);
    offset = (unsigned int)y * 32u;
    for (x = first; x <= last; ++x)
        ZX_ATTR_BASE[offset + x] = attribute;
}

static void clear_screen(void)
{
    putchar(12);
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

static unsigned int title_seed(void)
{
    unsigned int seed;
    int key;

    seed = 0xACE1u;
    in_wait_nokey();
    key = 0;
    while (key == 0) {
        if ((seed & 1u) != 0u)
            seed = (unsigned int)((seed >> 1) ^ 0xB400u);
        else
            seed >>= 1;
        key = in_inkey();
    }
    in_wait_nokey();
    seed ^= (unsigned int)((unsigned int)key << 8);
    if (seed == 0u)
        seed = 1u;
    return seed;
}

static void show_title(void)
{
    clear_screen();
    fill_attributes(ATTR(ZX_CYAN, ZX_BLACK));
    set_attribute_span(1u, 0u, 63u, ATTR(ZX_YELLOW, ZX_BLACK));
    set_attribute_span(2u, 0u, 63u, ATTR(ZX_YELLOW, ZX_BLACK));
    set_attribute_span(5u, 12u, 51u, ATTR(ZX_RED, ZX_BLACK));
    set_attribute_span(17u, 0u, 63u, ATTR(ZX_GREEN, ZX_BLACK));
    set_attribute_span(20u, 0u, 63u, ATTR(ZX_YELLOW, ZX_BLACK));

    screen_center(1u, "R O B O T S");
    screen_center(2u, "THE CLASSIC BSD GAME");
    screen_center(4u, "/--------------------------------------\\");
    screen_center(5u, "|    +       +       +       +         |");
    screen_center(6u, "|                                      |");
    screen_center(7u, "|             \\    |    /             |");
    screen_center(8u, "|                  @                   |");
    screen_center(9u, "|             /    |    \\             |");
    screen_center(10u, "|                                      |");
    screen_center(11u, "|       *       *       *       *      |");
    screen_center(12u, "\\--------------------------------------/");
    screen_center(15u, "NO WEAPONS. MAKE THE ROBOTS COLLIDE.");
    screen_center(17u, "59 X 22 ORIGINAL ARENA - PURE 48K");
    screen_center(19u, "VI KEYS OR 1-9   T TELEPORT   W WAIT");
    screen_center(20u, "PRESS ANY KEY");
    screen_center(22u, "ORIGINAL GAME BY KEN ARNOLD - I: HELP IN GAME");
    board_visible = 0u;
}

static void show_help(void)
{
    int key;

    clear_screen();
    fill_attributes(ATTR(ZX_WHITE, ZX_BLACK));
    set_attribute_span(0u, 0u, 63u, ATTR(ZX_YELLOW, ZX_BLUE));
    set_attribute_span(22u, 0u, 63u, ATTR(ZX_CYAN, ZX_BLACK));

    screen_center(0u, "HOW TO PLAY");
    screen_text(2u, 2u, "YOU ARE @. ROBOTS ARE +. JUNK HEAPS ARE *.");
    screen_text(2u, 3u, "EVERY + TAKES ONE STEP TOWARD @ AFTER YOUR TURN.");
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
    screen_put(0u, 0u, '+');
    screen_put(FIELD_RIGHT, 0u, '+');
    screen_put(0u, 23u, '+');
    screen_put(FIELD_RIGHT, 23u, '+');
    for (y = 1u; y < 23u; ++y) {
        screen_put(0u, y, '|');
        screen_put(FIELD_RIGHT, y, '|');
    }
    memset(previous_cells, 0xff, sizeof(previous_cells));
}

static unsigned char pair_attribute(unsigned char y, unsigned char pair)
{
    unsigned char screen_x;
    unsigned char i;
    unsigned char value;
    unsigned char best;

    if (y == 0u || y == 23u)
        return ATTR(ZX_CYAN, ZX_BLACK);

    best = 0u;
    for (i = 0u; i < 2u; ++i) {
        screen_x = (unsigned char)(pair * 2u + i);
        if (screen_x == 0u || screen_x == FIELD_RIGHT) {
            if (best < 1u)
                best = 1u;
        } else if (screen_x >= FIELD_LEFT && screen_x < FIELD_RIGHT) {
            value = render_cells[(unsigned char)(y - 1u)]
                                [(unsigned char)(screen_x - FIELD_LEFT)];
            if (value == '@')
                best = 4u;
            else if (value == '+' && best < 3u)
                best = 3u;
            else if (value == '*' && best < 2u)
                best = 2u;
        }
    }

    if (best == 4u)
        return ATTR(ZX_YELLOW, ZX_BLACK);
    if (best == 3u)
        return ATTR(ZX_RED, ZX_BLACK);
    if (best == 2u)
        return ATTR(ZX_WHITE, ZX_BLACK);
    if (best == 1u)
        return ATTR(ZX_CYAN, ZX_BLACK);
    return ATTR(ZX_GREEN, ZX_BLACK);
}

static void draw_status(void)
{
    unsigned char x;

    for (x = 1u; x < FIELD_RIGHT; ++x)
        screen_put(x, 0u, '-');
    screen_put(0u, 0u, '+');
    screen_put(FIELD_RIGHT, 0u, '+');
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
    unsigned char pair;

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
                screen_put((unsigned char)(x + FIELD_LEFT),
                           (unsigned char)(y + 1u), render_cells[y][x]);
                previous_cells[y][x] = render_cells[y][x];
            }
        }
    }

    for (y = 0u; y < SCREEN_ROWS; ++y) {
        for (pair = 0u; pair < 31u; ++pair)
            ZX_ATTR_BASE[(unsigned int)y * 32u + pair] =
                pair_attribute(y, pair);
    }
    ZX_ATTR_BASE[31u] = ATTR(ZX_RED, ZX_BLACK);
}

static void show_notice(unsigned char value)
{
    screen_put(NOTICE_COLUMN, 0u, value);
    ZX_ATTR_BASE[31u] = ATTR(ZX_RED, ZX_BLACK);
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
        set_attribute_span(y, 9u, 54u, ATTR(ZX_YELLOW, ZX_BLUE));
    }
    for (x = 9u; x <= 54u; ++x) {
        screen_put(x, 7u, '-');
        screen_put(x, 16u, '-');
    }
    screen_put(9u, 7u, '+');
    screen_put(54u, 7u, '+');
    screen_put(9u, 16u, '+');
    screen_put(54u, 16u, '+');
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

    for (;;) {
        show_title();
        seed = title_seed();
        robots_game_init(&game, seed);
        board_visible = 0u;
        if (play_game() != 0u)
            break;
    }

    clear_screen();
    fill_attributes(ATTR(ZX_CYAN, ZX_BLACK));
    screen_center(10u, "THANKS FOR PLAYING ROBOTS");
    screen_center(12u, "RANDOMIZE USR TO PLAY AGAIN");
    return 0;
}
