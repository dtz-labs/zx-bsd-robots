#ifndef ROBOTS_GAME_H
#define ROBOTS_GAME_H

/* The original BSD play field: x = 0..58, y = 0..21. */
#define ROBOTS_GAME_WIDTH 59
#define ROBOTS_GAME_HEIGHT 22
#define ROBOTS_GAME_MAX_ROBOTS 40
#define ROBOTS_GAME_MAX_HEAPS 40

#define ROBOTS_GAME_STATUS_RUNNING 1
#define ROBOTS_GAME_STATUS_CLEARED 2
#define ROBOTS_GAME_STATUS_DEAD 3

#define ROBOTS_GAME_RESULT_REJECTED 0
#define ROBOTS_GAME_RESULT_RUNNING 1
#define ROBOTS_GAME_RESULT_CLEARED 2
#define ROBOTS_GAME_RESULT_DEAD 3

#define ROBOTS_GAME_CELL_EMPTY 0
#define ROBOTS_GAME_CELL_PLAYER 1
#define ROBOTS_GAME_CELL_ROBOT 2
#define ROBOTS_GAME_CELL_HEAP 3

typedef struct RobotsPosition {
    signed char x;
    signed char y;
} RobotsPosition;

/*
 * All storage belongs to the caller.  The arrays are compact: entries below
 * robot_count and heap_count are live.  rng_lo/rng_hi are deliberately part
 * of the state so a saved game and a test can reproduce future teleports.
 */
typedef struct RobotsGame {
    RobotsPosition player;
    RobotsPosition robots[ROBOTS_GAME_MAX_ROBOTS];
    RobotsPosition heaps[ROBOTS_GAME_MAX_HEAPS];
    unsigned int level;
    unsigned long score;
    unsigned char robot_count;
    unsigned char heap_count;
    unsigned char status;
    unsigned char waiting;
    unsigned char wait_kills;
    unsigned char last_wait_bonus;
    unsigned char rng_lo;
    unsigned char rng_hi;
} RobotsGame;

void robots_game_init(RobotsGame *game, unsigned int seed);

/* Starts the following level only after the current one has been cleared. */
unsigned char robots_game_next_level(RobotsGame *game);

/* dx and dy must each be in -1..1.  A zero/zero move is allowed if safe. */
unsigned char robots_game_move(RobotsGame *game,
                               signed char dx, signed char dy);

/* Teleportation chooses an empty square but deliberately skips move safety. */
unsigned char robots_game_teleport(RobotsGame *game);

/* Begin does not advance time; each step advances the robots exactly once. */
unsigned char robots_game_wait_begin(RobotsGame *game);
unsigned char robots_game_wait_step(RobotsGame *game);

/* Out-of-bounds coordinates are reported as empty. */
unsigned char robots_game_cell(const RobotsGame *game,
                               signed char x, signed char y);

#endif
