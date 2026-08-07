#include <stdio.h>
#include <stdlib.h>

#include "robots_input.h"

static void fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void expect_decode(unsigned char state, RobotsInputEvent expected_event,
                          signed char expected_dx, signed char expected_dy,
                          const char *message)
{
    RobotsInputEvent event;
    signed char dx;
    signed char dy;

    dx = 99;
    dy = 99;
    event = robots_input_decode_state(state, &dx, &dy);
    if (event != expected_event || dx != expected_dx || dy != expected_dy)
        fail(message);
}

static void test_decode(void)
{
    expect_decode(0u, ROBOTS_INPUT_NONE, 0, 0, "neutral state");
    expect_decode(ROBOTS_INPUT_STICK_UP, ROBOTS_INPUT_MOVE, 0, -1,
                  "up");
    expect_decode(ROBOTS_INPUT_STICK_DOWN, ROBOTS_INPUT_MOVE, 0, 1,
                  "down");
    expect_decode(ROBOTS_INPUT_STICK_LEFT, ROBOTS_INPUT_MOVE, -1, 0,
                  "left");
    expect_decode(ROBOTS_INPUT_STICK_RIGHT, ROBOTS_INPUT_MOVE, 1, 0,
                  "right");
    expect_decode(ROBOTS_INPUT_STICK_UP | ROBOTS_INPUT_STICK_LEFT,
                  ROBOTS_INPUT_MOVE, -1, -1, "up-left diagonal");
    expect_decode(ROBOTS_INPUT_STICK_UP | ROBOTS_INPUT_STICK_RIGHT,
                  ROBOTS_INPUT_MOVE, 1, -1, "up-right diagonal");
    expect_decode(ROBOTS_INPUT_STICK_DOWN | ROBOTS_INPUT_STICK_LEFT,
                  ROBOTS_INPUT_MOVE, -1, 1, "down-left diagonal");
    expect_decode(ROBOTS_INPUT_STICK_DOWN | ROBOTS_INPUT_STICK_RIGHT,
                  ROBOTS_INPUT_MOVE, 1, 1, "down-right diagonal");
    expect_decode(ROBOTS_INPUT_STICK_UP | ROBOTS_INPUT_STICK_DOWN,
                  ROBOTS_INPUT_NONE, 0, 0, "opposite vertical directions");
    expect_decode(ROBOTS_INPUT_STICK_LEFT | ROBOTS_INPUT_STICK_RIGHT,
                  ROBOTS_INPUT_NONE, 0, 0, "opposite horizontal directions");
    expect_decode(ROBOTS_INPUT_STICK_FIRE | ROBOTS_INPUT_STICK_UP,
                  ROBOTS_INPUT_TELEPORT, 0, 0, "fire takes precedence");
    expect_decode(0x70u, ROBOTS_INPUT_NONE, 0, 0,
                  "unused z88dk fire bits are ignored");
}

static void test_release_edge(void)
{
    RobotsInputEvent event;
    unsigned char was_active;
    signed char dx;
    signed char dy;

    was_active = 1u;
    event = robots_input_edge_state(ROBOTS_INPUT_STICK_RIGHT, &was_active,
                                    &dx, &dy);
    if (event != ROBOTS_INPUT_NONE)
        fail("reset state suppresses a joystick already held");

    event = robots_input_edge_state(0u, &was_active, &dx, &dy);
    if (event != ROBOTS_INPUT_NONE || was_active != 0u)
        fail("neutral state rearms joystick");

    event = robots_input_edge_state(ROBOTS_INPUT_STICK_RIGHT, &was_active,
                                    &dx, &dy);
    if (event != ROBOTS_INPUT_MOVE || dx != 1 || dy != 0 ||
        was_active == 0u)
        fail("first deflection creates one move");

    event = robots_input_edge_state(ROBOTS_INPUT_STICK_UP, &was_active,
                                    &dx, &dy);
    if (event != ROBOTS_INPUT_NONE)
        fail("changing direction without release does not repeat");

    (void)robots_input_edge_state(0u, &was_active, &dx, &dy);
    event = robots_input_edge_state(ROBOTS_INPUT_STICK_FIRE, &was_active,
                                    &dx, &dy);
    if (event != ROBOTS_INPUT_TELEPORT || dx != 0 || dy != 0)
        fail("fire creates teleport after release");
}

static void test_matrix_mode(void)
{
    robots_input_init(ROBOTS_MATRIX_JOYSTICK_CURSOR);
    if (robots_input_get_matrix_joystick() != ROBOTS_MATRIX_JOYSTICK_CURSOR)
        fail("cursor matrix mode");

    robots_input_set_matrix_joystick(ROBOTS_MATRIX_JOYSTICK_SINCLAIR1);
    if (robots_input_get_matrix_joystick() !=
        ROBOTS_MATRIX_JOYSTICK_SINCLAIR1)
        fail("Sinclair 1 matrix mode");

    robots_input_set_matrix_joystick((RobotsMatrixJoystick)99);
    if (robots_input_get_matrix_joystick() != ROBOTS_MATRIX_JOYSTICK_OFF)
        fail("invalid matrix mode becomes off");

    if (robots_input_get_kempston() != 0u)
        fail("Kempston must default off because an absent port floats");
    robots_input_set_kempston(1u);
    if (robots_input_get_kempston() == 0u)
        fail("Kempston enable");
    robots_input_set_kempston(0u);
    if (robots_input_get_kempston() != 0u)
        fail("Kempston disable");
}

int main(void)
{
    test_decode();
    test_release_edge();
    test_matrix_mode();
    puts("input tests passed");
    return 0;
}
