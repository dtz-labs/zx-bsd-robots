#include "robots_game.h"

static unsigned char position_equal(RobotsPosition a, RobotsPosition b)
{
    return (unsigned char)(a.x == b.x && a.y == b.y);
}

static unsigned char in_bounds(signed char x, signed char y)
{
    if (x < 0 || y < 0)
        return 0;
    if (x >= ROBOTS_GAME_WIDTH || y >= ROBOTS_GAME_HEIGHT)
        return 0;
    return 1;
}

static signed char direction(signed char from, signed char to)
{
    if (from < to)
        return 1;
    if (from > to)
        return -1;
    return 0;
}

static unsigned int random_value(RobotsGame *game)
{
    unsigned long value;

    value = ((unsigned long)game->rng_hi << 8) | game->rng_lo;
    value ^= value << 7;
    value &= 65535UL;
    value ^= value >> 9;
    value &= 65535UL;
    value ^= value << 8;
    value &= 65535UL;

    game->rng_lo = (unsigned char)(value & 255UL);
    game->rng_hi = (unsigned char)((value >> 8) & 255UL);
    return (unsigned int)value;
}

static unsigned char robot_at(const RobotsGame *game, RobotsPosition position)
{
    unsigned char i;

    for (i = 0; i < game->robot_count; ++i) {
        if (position_equal(game->robots[i], position))
            return 1;
    }
    return 0;
}

static unsigned char heap_at(const RobotsGame *game, RobotsPosition position)
{
    unsigned char i;

    for (i = 0; i < game->heap_count; ++i) {
        if (position_equal(game->heaps[i], position))
            return 1;
    }
    return 0;
}

static unsigned char blocked(const RobotsGame *game, RobotsPosition position)
{
    return (unsigned char)(robot_at(game, position) || heap_at(game, position));
}

static RobotsPosition random_empty_position(RobotsGame *game)
{
    RobotsPosition position;
    unsigned int cell;

    do {
        cell = (unsigned int)(random_value(game) %
                              (ROBOTS_GAME_WIDTH * ROBOTS_GAME_HEIGHT));
        position.x = (signed char)(cell % ROBOTS_GAME_WIDTH);
        position.y = (signed char)(cell / ROBOTS_GAME_WIDTH);
    } while (blocked(game, position));

    return position;
}

static void start_level(RobotsGame *game)
{
    unsigned char count;
    unsigned char i;

    if (game->level >= 4)
        count = ROBOTS_GAME_MAX_ROBOTS;
    else
        count = (unsigned char)(game->level * 10);

    game->robot_count = 0;
    game->heap_count = 0;
    game->waiting = 0;
    game->wait_kills = 0;
    game->last_wait_bonus = 0;
    game->status = ROBOTS_GAME_STATUS_RUNNING;

    for (i = 0; i < count; ++i) {
        game->robots[game->robot_count] = random_empty_position(game);
        ++game->robot_count;
    }
    game->player = random_empty_position(game);
}

void robots_game_init(RobotsGame *game, unsigned int seed)
{
    unsigned char i;

    game->player.x = 0;
    game->player.y = 0;
    for (i = 0; i < ROBOTS_GAME_MAX_ROBOTS; ++i) {
        game->robots[i].x = 0;
        game->robots[i].y = 0;
        game->heaps[i].x = 0;
        game->heaps[i].y = 0;
    }
    game->level = 1;
    game->score = 0;
    game->robot_count = 0;
    game->heap_count = 0;
    game->status = ROBOTS_GAME_STATUS_RUNNING;
    game->waiting = 0;
    game->wait_kills = 0;
    game->last_wait_bonus = 0;
    game->rng_lo = (unsigned char)((unsigned long)seed & 255UL);
    game->rng_hi = (unsigned char)(((unsigned long)seed >> 8) & 255UL);
    if (game->rng_lo == 0 && game->rng_hi == 0) {
        game->rng_lo = 0xe1;
        game->rng_hi = 0xac;
    }
    start_level(game);
}

unsigned char robots_game_next_level(RobotsGame *game)
{
    if (game->status != ROBOTS_GAME_STATUS_CLEARED)
        return 0;
    if (game->level != 65535U)
        ++game->level;
    start_level(game);
    return 1;
}

static RobotsPosition robot_destination(RobotsPosition robot,
                                        RobotsPosition player)
{
    robot.x = (signed char)(robot.x + direction(robot.x, player.x));
    robot.y = (signed char)(robot.y + direction(robot.y, player.y));
    return robot;
}

static unsigned char destination_is_eaten(const RobotsGame *game,
                                           RobotsPosition player)
{
    unsigned char i;

    for (i = 0; i < game->robot_count; ++i) {
        if (position_equal(robot_destination(game->robots[i], player), player))
            return 1;
    }
    return 0;
}

static void add_heap(RobotsGame *game, RobotsPosition position)
{
    if (heap_at(game, position))
        return;
    if (game->heap_count < ROBOTS_GAME_MAX_HEAPS) {
        game->heaps[game->heap_count] = position;
        ++game->heap_count;
    }
}

static unsigned char finish_turn(RobotsGame *game,
                                 unsigned char player_hit)
{
    if (player_hit) {
        game->status = ROBOTS_GAME_STATUS_DEAD;
        game->waiting = 0;
        game->last_wait_bonus = 0;
        return ROBOTS_GAME_RESULT_DEAD;
    }

    if (game->robot_count == 0) {
        game->status = ROBOTS_GAME_STATUS_CLEARED;
        if (game->waiting) {
            game->last_wait_bonus = game->wait_kills;
            game->score += game->wait_kills;
            game->waiting = 0;
        }
        else
            game->last_wait_bonus = 0;
        return ROBOTS_GAME_RESULT_CLEARED;
    }

    return ROBOTS_GAME_RESULT_RUNNING;
}

static unsigned char advance_robots(RobotsGame *game)
{
    RobotsPosition moved[ROBOTS_GAME_MAX_ROBOTS];
    unsigned char old_count;
    unsigned char new_count;
    unsigned char player_hit;
    unsigned char killed;
    unsigned char collision;
    unsigned char i;
    unsigned char j;

    old_count = game->robot_count;
    for (i = 0; i < old_count; ++i)
        moved[i] = robot_destination(game->robots[i], game->player);

    player_hit = 0;
    for (i = 0; i < old_count; ++i) {
        if (position_equal(moved[i], game->player)) {
            player_hit = 1;
            break;
        }
    }

    new_count = 0;
    killed = 0;
    for (i = 0; i < old_count; ++i) {
        /* In BSD robots, reaching the player takes precedence over a crash. */
        if (position_equal(moved[i], game->player)) {
            game->robots[new_count] = moved[i];
            ++new_count;
            continue;
        }

        if (heap_at(game, moved[i])) {
            ++killed;
            continue;
        }

        collision = 0;
        for (j = 0; j < old_count; ++j) {
            if (i != j && position_equal(moved[i], moved[j])) {
                collision = 1;
                break;
            }
        }
        if (collision) {
            add_heap(game, moved[i]);
            ++killed;
        }
        else {
            game->robots[new_count] = moved[i];
            ++new_count;
        }
    }

    game->robot_count = new_count;
    game->score += (unsigned long)killed * 10UL;
    if (game->waiting)
        game->wait_kills = (unsigned char)(game->wait_kills + killed);

    return finish_turn(game, player_hit);
}

unsigned char robots_game_move(RobotsGame *game,
                               signed char dx, signed char dy)
{
    RobotsPosition destination;

    if (game->status != ROBOTS_GAME_STATUS_RUNNING || game->waiting)
        return ROBOTS_GAME_RESULT_REJECTED;
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return ROBOTS_GAME_RESULT_REJECTED;

    destination.x = (signed char)(game->player.x + dx);
    destination.y = (signed char)(game->player.y + dy);
    if (!in_bounds(destination.x, destination.y))
        return ROBOTS_GAME_RESULT_REJECTED;
    if (blocked(game, destination))
        return ROBOTS_GAME_RESULT_REJECTED;
    if (destination_is_eaten(game, destination))
        return ROBOTS_GAME_RESULT_REJECTED;

    game->player = destination;
    return advance_robots(game);
}

unsigned char robots_game_teleport(RobotsGame *game)
{
    if (game->status != ROBOTS_GAME_STATUS_RUNNING || game->waiting)
        return ROBOTS_GAME_RESULT_REJECTED;

    game->player = random_empty_position(game);
    return advance_robots(game);
}

unsigned char robots_game_wait_begin(RobotsGame *game)
{
    if (game->status != ROBOTS_GAME_STATUS_RUNNING || game->waiting)
        return ROBOTS_GAME_RESULT_REJECTED;
    game->waiting = 1;
    game->wait_kills = 0;
    game->last_wait_bonus = 0;
    return ROBOTS_GAME_RESULT_RUNNING;
}

unsigned char robots_game_wait_step(RobotsGame *game)
{
    if (game->status != ROBOTS_GAME_STATUS_RUNNING || !game->waiting)
        return ROBOTS_GAME_RESULT_REJECTED;
    return advance_robots(game);
}

unsigned char robots_game_cell(const RobotsGame *game,
                               signed char x, signed char y)
{
    RobotsPosition position;

    if (!in_bounds(x, y))
        return ROBOTS_GAME_CELL_EMPTY;
    position.x = x;
    position.y = y;
    if (position_equal(game->player, position))
        return ROBOTS_GAME_CELL_PLAYER;
    if (robot_at(game, position))
        return ROBOTS_GAME_CELL_ROBOT;
    if (heap_at(game, position))
        return ROBOTS_GAME_CELL_HEAP;
    return ROBOTS_GAME_CELL_EMPTY;
}
