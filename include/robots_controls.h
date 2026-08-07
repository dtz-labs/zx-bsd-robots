#ifndef ROBOTS_CONTROLS_H
#define ROBOTS_CONTROLS_H

typedef enum RobotsControlAction {
    ROBOTS_CONTROL_NONE = 0,
    ROBOTS_CONTROL_MOVE = 1,
    ROBOTS_CONTROL_TELEPORT = 2,
    ROBOTS_CONTROL_WAIT = 3,
    ROBOTS_CONTROL_HELP = 4,
    ROBOTS_CONTROL_QUIT = 5
} RobotsControlAction;

/* Portable keyboard decoder. A run with dx=dy=0 is safe standing. */
RobotsControlAction robots_control_decode(int key, signed char *dx,
                                          signed char *dy,
                                          unsigned char *run);

#endif
