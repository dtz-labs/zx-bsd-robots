#include "robots_controls.h"

RobotsControlAction robots_control_decode(int key, signed char *dx,
                                          signed char *dy,
                                          unsigned char *run)
{
    *dx = 0;
    *dy = 0;
    *run = 0u;

    switch (key) {
    case 'i': case 'I': case '?': return ROBOTS_CONTROL_HELP;
    case 'q': case 'Q': return ROBOTS_CONTROL_QUIT;
    case 't': case 'T': return ROBOTS_CONTROL_TELEPORT;
    case 'w': case 'W': return ROBOTS_CONTROL_WAIT;

    case 'y': case '7': *dx = -1; *dy = -1; return ROBOTS_CONTROL_MOVE;
    case 'k': case '8': *dy = -1; return ROBOTS_CONTROL_MOVE;
    case 'u': case '9': *dx = 1; *dy = -1; return ROBOTS_CONTROL_MOVE;
    case 'h': case '4': *dx = -1; return ROBOTS_CONTROL_MOVE;
    case 'l': case '6': *dx = 1; return ROBOTS_CONTROL_MOVE;
    case 'b': case '1': *dx = -1; *dy = 1; return ROBOTS_CONTROL_MOVE;
    case 'j': case '2': *dy = 1; return ROBOTS_CONTROL_MOVE;
    case 'n': case '3': *dx = 1; *dy = 1; return ROBOTS_CONTROL_MOVE;
    case ' ': case '.': case '5': case 13: return ROBOTS_CONTROL_MOVE;

    case 'Y': *dx = -1; *dy = -1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'K': *dy = -1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'U': *dx = 1; *dy = -1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'H': *dx = -1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'L': *dx = 1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'B': *dx = -1; *dy = 1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'J': *dy = 1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 'N': *dx = 1; *dy = 1; *run = 1u; return ROBOTS_CONTROL_MOVE;
    case 's': case 'S': case '>': *run = 1u; return ROBOTS_CONTROL_MOVE;
    default: return ROBOTS_CONTROL_NONE;
    }
}
