#include <stdio.h>

#include "robots_controls.h"

static int expect(int key, RobotsControlAction expected_action,
                  signed char expected_dx, signed char expected_dy,
                  unsigned char expected_run)
{
    RobotsControlAction action;
    signed char dx;
    signed char dy;
    unsigned char run;

    action = robots_control_decode(key, &dx, &dy, &run);
    if (action != expected_action || dx != expected_dx || dy != expected_dy ||
        run != expected_run) {
        fprintf(stderr, "unexpected control mapping for key %d\n", key);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const char original_keys[] = "ykuhlbjn";
    static const char numeric_keys[] = "78946123";
    static const signed char dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const signed char dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    unsigned char i;

    for (i = 0u; i < 8u; ++i) {
        if (!expect(original_keys[i], ROBOTS_CONTROL_MOVE, dx[i], dy[i], 0u) ||
            !expect(numeric_keys[i], ROBOTS_CONTROL_MOVE, dx[i], dy[i], 0u) ||
            !expect(original_keys[i] - 'a' + 'A', ROBOTS_CONTROL_MOVE,
                    dx[i], dy[i], 1u))
            return 1;
    }

    if (!expect(' ', ROBOTS_CONTROL_MOVE, 0, 0, 0u) ||
        !expect('.', ROBOTS_CONTROL_MOVE, 0, 0, 0u) ||
        !expect('5', ROBOTS_CONTROL_MOVE, 0, 0, 0u) ||
        !expect('S', ROBOTS_CONTROL_MOVE, 0, 0, 1u) ||
        !expect('T', ROBOTS_CONTROL_TELEPORT, 0, 0, 0u) ||
        !expect('t', ROBOTS_CONTROL_TELEPORT, 0, 0, 0u) ||
        !expect('W', ROBOTS_CONTROL_WAIT, 0, 0, 0u) ||
        !expect('I', ROBOTS_CONTROL_HELP, 0, 0, 0u) ||
        !expect('Q', ROBOTS_CONTROL_QUIT, 0, 0, 0u) ||
        !expect('0', ROBOTS_CONTROL_NONE, 0, 0, 0u))
        return 1;

    puts("all keyboard-control tests passed");
    return 0;
}
