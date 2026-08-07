#include <stdio.h>
#include <string.h>

#include "robots_game.h"

static int failures;

#define CHECK(condition) check((condition), #condition, __LINE__)

static void check(int condition, const char *text, int line)
{
    if (!condition) {
        fprintf(stderr, "line %d: check failed: %s\n", line, text);
        ++failures;
    }
}

static RobotsPosition position(signed char x, signed char y)
{
    RobotsPosition result;

    result.x = x;
    result.y = y;
    return result;
}

static int same_position(RobotsPosition a, RobotsPosition b)
{
    return a.x == b.x && a.y == b.y;
}

static void empty_game(RobotsGame *game)
{
    memset(game, 0, sizeof(*game));
    game->level = 1;
    game->status = ROBOTS_GAME_STATUS_RUNNING;
    game->rng_lo = 1;
}

static void test_level_sizes_and_placement(void)
{
    RobotsGame game;
    unsigned char i;
    unsigned char j;

    robots_game_init(&game, 1234U);
    CHECK(game.level == 1);
    CHECK(game.robot_count == 10);
    CHECK(game.heap_count == 0);
    CHECK(game.status == ROBOTS_GAME_STATUS_RUNNING);
    CHECK(game.player.x >= 0 && game.player.x < ROBOTS_GAME_WIDTH);
    CHECK(game.player.y >= 0 && game.player.y < ROBOTS_GAME_HEIGHT);
    for (i = 0; i < game.robot_count; ++i) {
        CHECK(game.robots[i].x >= 0 &&
              game.robots[i].x < ROBOTS_GAME_WIDTH);
        CHECK(game.robots[i].y >= 0 &&
              game.robots[i].y < ROBOTS_GAME_HEIGHT);
        CHECK(!same_position(game.player, game.robots[i]));
        for (j = (unsigned char)(i + 1); j < game.robot_count; ++j)
            CHECK(!same_position(game.robots[i], game.robots[j]));
    }

    CHECK(robots_game_next_level(&game) == 0);
    game.status = ROBOTS_GAME_STATUS_CLEARED;
    CHECK(robots_game_next_level(&game) == 1);
    CHECK(game.level == 2 && game.robot_count == 20);
    game.status = ROBOTS_GAME_STATUS_CLEARED;
    CHECK(robots_game_next_level(&game) == 1);
    CHECK(game.level == 3 && game.robot_count == 30);
    game.status = ROBOTS_GAME_STATUS_CLEARED;
    CHECK(robots_game_next_level(&game) == 1);
    CHECK(game.level == 4 && game.robot_count == 40);
    game.status = ROBOTS_GAME_STATUS_CLEARED;
    CHECK(robots_game_next_level(&game) == 1);
    CHECK(game.level == 5 && game.robot_count == 40);
}

static void test_simultaneous_collision_and_heap(void)
{
    RobotsGame game;
    unsigned char result;

    empty_game(&game);
    game.player = position(5, 5);
    game.robot_count = 2;
    game.robots[0] = position(1, 4);
    game.robots[1] = position(1, 6);

    result = robots_game_move(&game, 0, 0);
    CHECK(result == ROBOTS_GAME_RESULT_CLEARED);
    CHECK(game.robot_count == 0);
    CHECK(game.heap_count == 1);
    CHECK(same_position(game.heaps[0], position(2, 5)));
    CHECK(game.score == 20UL);

    empty_game(&game);
    game.player = position(5, 5);
    game.heap_count = 1;
    game.heaps[0] = position(2, 2);
    game.robot_count = 1;
    game.robots[0] = position(1, 1);

    result = robots_game_move(&game, 0, 0);
    CHECK(result == ROBOTS_GAME_RESULT_CLEARED);
    CHECK(game.robot_count == 0);
    CHECK(game.heap_count == 1);
    CHECK(same_position(game.heaps[0], position(2, 2)));
    CHECK(game.score == 10UL);
}

static void test_safe_move_rejections(void)
{
    RobotsGame game;
    RobotsPosition old_player;

    empty_game(&game);
    game.player = position(0, 0);
    game.robot_count = 1;
    game.robots[0] = position(10, 10);
    old_player = game.player;
    CHECK(robots_game_move(&game, -1, 0) ==
          ROBOTS_GAME_RESULT_REJECTED);
    CHECK(same_position(game.player, old_player));
    CHECK(same_position(game.robots[0], position(10, 10)));

    empty_game(&game);
    game.player = position(5, 5);
    game.robot_count = 1;
    game.robots[0] = position(6, 5);
    CHECK(robots_game_move(&game, 1, 0) ==
          ROBOTS_GAME_RESULT_REJECTED);

    empty_game(&game);
    game.player = position(5, 5);
    game.heap_count = 1;
    game.heaps[0] = position(6, 5);
    game.robot_count = 1;
    game.robots[0] = position(20, 20);
    CHECK(robots_game_move(&game, 1, 0) ==
          ROBOTS_GAME_RESULT_REJECTED);

    empty_game(&game);
    game.player = position(5, 5);
    game.robot_count = 1;
    game.robots[0] = position(7, 5);
    CHECK(robots_game_move(&game, 1, 0) ==
          ROBOTS_GAME_RESULT_REJECTED);
    CHECK(same_position(game.robots[0], position(7, 5)));

    empty_game(&game);
    game.player = position(5, 5);
    game.robot_count = 1;
    game.robots[0] = position(10, 10);
    CHECK(robots_game_move(&game, 1, 0) ==
          ROBOTS_GAME_RESULT_RUNNING);
    CHECK(same_position(game.player, position(6, 5)));
    CHECK(same_position(game.robots[0], position(9, 9)));
}

static void test_teleport_is_deterministic_empty_and_risky(void)
{
    RobotsGame first;
    RobotsGame second;
    unsigned long seed;
    unsigned char result;
    int found_dead;

    empty_game(&first);
    first.player = position(0, 0);
    first.robot_count = 1;
    first.robots[0] = position(30, 10);
    first.heap_count = 1;
    first.heaps[0] = position(40, 15);
    first.rng_lo = 0x34;
    first.rng_hi = 0x12;
    second = first;
    result = robots_game_teleport(&first);
    CHECK(result == robots_game_teleport(&second));
    CHECK(same_position(first.player, second.player));
    CHECK(first.rng_lo == second.rng_lo && first.rng_hi == second.rng_hi);
    CHECK(!same_position(first.player, position(30, 10)));
    CHECK(!same_position(first.player, position(40, 15)));

    found_dead = 0;
    for (seed = 1; seed <= 65535UL; ++seed) {
        empty_game(&first);
        first.player = position(0, 0);
        first.robot_count = 1;
        first.robots[0] = position(30, 10);
        first.rng_lo = (unsigned char)(seed & 255UL);
        first.rng_hi = (unsigned char)((seed >> 8) & 255UL);
        if (robots_game_teleport(&first) == ROBOTS_GAME_RESULT_DEAD) {
            found_dead = 1;
            break;
        }
    }
    CHECK(found_dead);
}

static void test_wait_animation_and_bonus(void)
{
    RobotsGame game;
    RobotsPosition before;
    unsigned char result;

    empty_game(&game);
    game.player = position(5, 5);
    game.robot_count = 2;
    game.robots[0] = position(1, 4);
    game.robots[1] = position(1, 6);
    before = game.robots[0];

    CHECK(robots_game_wait_begin(&game) == ROBOTS_GAME_RESULT_RUNNING);
    CHECK(game.waiting == 1);
    CHECK(same_position(game.robots[0], before));
    CHECK(robots_game_move(&game, 0, 0) ==
          ROBOTS_GAME_RESULT_REJECTED);
    result = robots_game_wait_step(&game);
    CHECK(result == ROBOTS_GAME_RESULT_CLEARED);
    CHECK(game.waiting == 0);
    CHECK(game.wait_kills == 2);
    CHECK(game.last_wait_bonus == 2);
    CHECK(game.score == 22UL);

    empty_game(&game);
    game.player = position(5, 5);
    game.robot_count = 3;
    game.robots[0] = position(4, 5);
    game.robots[1] = position(1, 4);
    game.robots[2] = position(1, 6);
    CHECK(robots_game_wait_begin(&game) == ROBOTS_GAME_RESULT_RUNNING);
    result = robots_game_wait_step(&game);
    CHECK(result == ROBOTS_GAME_RESULT_DEAD);
    CHECK(game.wait_kills == 2);
    CHECK(game.last_wait_bonus == 0);
    CHECK(game.score == 20UL);
}

static void test_cell_query(void)
{
    RobotsGame game;

    empty_game(&game);
    game.player = position(3, 4);
    game.robot_count = 1;
    game.robots[0] = position(8, 9);
    game.heap_count = 1;
    game.heaps[0] = position(10, 11);
    CHECK(robots_game_cell(&game, 3, 4) == ROBOTS_GAME_CELL_PLAYER);
    CHECK(robots_game_cell(&game, 8, 9) == ROBOTS_GAME_CELL_ROBOT);
    CHECK(robots_game_cell(&game, 10, 11) == ROBOTS_GAME_CELL_HEAP);
    CHECK(robots_game_cell(&game, 0, 0) == ROBOTS_GAME_CELL_EMPTY);
    CHECK(robots_game_cell(&game, -1, 0) == ROBOTS_GAME_CELL_EMPTY);
}

int main(void)
{
    test_level_sizes_and_placement();
    test_simultaneous_collision_and_heap();
    test_safe_move_rejections();
    test_teleport_is_deterministic_empty_and_risky();
    test_wait_animation_and_bonus();
    test_cell_query();

    if (failures != 0) {
        fprintf(stderr, "%d test check(s) failed\n", failures);
        return 1;
    }
    puts("all game-core tests passed");
    return 0;
}
