#include "robots_input.h"

#ifndef ROBOTS_INPUT_HOST_TEST
#include <input.h>

#if IN_STICK_UP != ROBOTS_INPUT_STICK_UP
#error "z88dk IN_STICK_UP does not match the Robots input contract"
#endif
#if IN_STICK_DOWN != ROBOTS_INPUT_STICK_DOWN
#error "z88dk IN_STICK_DOWN does not match the Robots input contract"
#endif
#if IN_STICK_LEFT != ROBOTS_INPUT_STICK_LEFT
#error "z88dk IN_STICK_LEFT does not match the Robots input contract"
#endif
#if IN_STICK_RIGHT != ROBOTS_INPUT_STICK_RIGHT
#error "z88dk IN_STICK_RIGHT does not match the Robots input contract"
#endif
#if IN_STICK_FIRE != ROBOTS_INPUT_STICK_FIRE
#error "z88dk IN_STICK_FIRE does not match the Robots input contract"
#endif
#endif

static RobotsMatrixJoystick selected_matrix_joystick;
static unsigned char kempston_enabled;
static unsigned char joystick_was_active;
static unsigned char last_matrix_active;

static RobotsMatrixJoystick normalise_matrix_joystick(
    RobotsMatrixJoystick matrix_joystick)
{
    if (matrix_joystick == ROBOTS_MATRIX_JOYSTICK_CURSOR ||
        matrix_joystick == ROBOTS_MATRIX_JOYSTICK_SINCLAIR1)
        return matrix_joystick;
    return ROBOTS_MATRIX_JOYSTICK_OFF;
}

void robots_input_init(RobotsMatrixJoystick matrix_joystick)
{
    selected_matrix_joystick = normalise_matrix_joystick(matrix_joystick);
    kempston_enabled = 0u;
    joystick_was_active = 1u;
    last_matrix_active = 0u;
}

void robots_input_set_matrix_joystick(RobotsMatrixJoystick matrix_joystick)
{
    selected_matrix_joystick = normalise_matrix_joystick(matrix_joystick);
    robots_input_reset();
}

RobotsMatrixJoystick robots_input_get_matrix_joystick(void)
{
    return selected_matrix_joystick;
}

void robots_input_set_kempston(unsigned char enabled)
{
    kempston_enabled = (unsigned char)(enabled != 0u);
    robots_input_reset();
}

unsigned char robots_input_get_kempston(void)
{
    return kempston_enabled;
}

void robots_input_reset(void)
{
    joystick_was_active = 1u;
    last_matrix_active = 0u;
}

RobotsInputEvent robots_input_decode_state(unsigned char state,
                                           signed char *dx,
                                           signed char *dy)
{
    signed char decoded_x;
    signed char decoded_y;

    decoded_x = 0;
    decoded_y = 0;
    state &= ROBOTS_INPUT_STICK_ACTIONS;

    if ((state & ROBOTS_INPUT_STICK_FIRE) != 0u) {
        *dx = 0;
        *dy = 0;
        return ROBOTS_INPUT_TELEPORT;
    }

    if ((state & ROBOTS_INPUT_STICK_LEFT) != 0u &&
        (state & ROBOTS_INPUT_STICK_RIGHT) == 0u)
        decoded_x = -1;
    else if ((state & ROBOTS_INPUT_STICK_RIGHT) != 0u &&
             (state & ROBOTS_INPUT_STICK_LEFT) == 0u)
        decoded_x = 1;

    if ((state & ROBOTS_INPUT_STICK_UP) != 0u &&
        (state & ROBOTS_INPUT_STICK_DOWN) == 0u)
        decoded_y = -1;
    else if ((state & ROBOTS_INPUT_STICK_DOWN) != 0u &&
             (state & ROBOTS_INPUT_STICK_UP) == 0u)
        decoded_y = 1;

    *dx = decoded_x;
    *dy = decoded_y;
    if (decoded_x == 0 && decoded_y == 0)
        return ROBOTS_INPUT_NONE;
    return ROBOTS_INPUT_MOVE;
}

RobotsInputEvent robots_input_edge_state(unsigned char state,
                                         unsigned char *was_active,
                                         signed char *dx,
                                         signed char *dy)
{
    state &= ROBOTS_INPUT_STICK_ACTIONS;
    *dx = 0;
    *dy = 0;

    if (state == 0u) {
        *was_active = 0u;
        return ROBOTS_INPUT_NONE;
    }
    if (*was_active != 0u)
        return ROBOTS_INPUT_NONE;

    *was_active = 1u;
    return robots_input_decode_state(state, dx, dy);
}

#ifndef ROBOTS_INPUT_HOST_TEST
static unsigned char read_matrix_joystick(void)
{
    if (selected_matrix_joystick == ROBOTS_MATRIX_JOYSTICK_CURSOR)
        return (unsigned char)in_stick_cursor();
    if (selected_matrix_joystick == ROBOTS_MATRIX_JOYSTICK_SINCLAIR1)
        return (unsigned char)in_stick_sinclair1();
    return 0u;
}

static unsigned char read_kempston_joystick(void)
{
    unsigned char state;

    state = (unsigned char)in_stick_kempston();

    /*
     * An absent interface can leave port 0x1f floating.  Reject states no
     * physical joystick can produce; in particular this filters the common
     * all-bits-high value instead of turning it into a phantom teleport.
     */
    if ((state & (ROBOTS_INPUT_STICK_UP | ROBOTS_INPUT_STICK_DOWN)) ==
        (ROBOTS_INPUT_STICK_UP | ROBOTS_INPUT_STICK_DOWN))
        return 0u;
    if ((state & (ROBOTS_INPUT_STICK_LEFT | ROBOTS_INPUT_STICK_RIGHT)) ==
        (ROBOTS_INPUT_STICK_LEFT | ROBOTS_INPUT_STICK_RIGHT))
        return 0u;
    return (unsigned char)(state & ROBOTS_INPUT_STICK_ACTIONS);
}
#endif

RobotsInputEvent robots_input_poll(signed char *dx, signed char *dy)
{
#ifdef ROBOTS_INPUT_HOST_TEST
    unsigned char matrix_state;
    unsigned char kempston_state;

    matrix_state = 0u;
    kempston_state = 0u;
#else
    unsigned char matrix_state;
    unsigned char kempston_state;

    matrix_state = read_matrix_joystick();
    if (kempston_enabled != 0u)
        kempston_state = read_kempston_joystick();
    else
        kempston_state = 0u;
#endif

    matrix_state &= ROBOTS_INPUT_STICK_ACTIONS;
    last_matrix_active = (unsigned char)(matrix_state != 0u);
    return robots_input_edge_state(
        (unsigned char)(matrix_state | kempston_state),
        &joystick_was_active, dx, dy);
}

unsigned char robots_input_matrix_active(void)
{
    return last_matrix_active;
}
