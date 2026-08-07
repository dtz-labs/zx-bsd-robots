#ifndef ROBOTS_INPUT_H
#define ROBOTS_INPUT_H

/* z88dk joystick states use F000RLDU, active high. */
#define ROBOTS_INPUT_STICK_UP 0x01u
#define ROBOTS_INPUT_STICK_DOWN 0x02u
#define ROBOTS_INPUT_STICK_LEFT 0x04u
#define ROBOTS_INPUT_STICK_RIGHT 0x08u
#define ROBOTS_INPUT_STICK_FIRE 0x80u
#define ROBOTS_INPUT_STICK_ACTIONS 0x8fu

typedef enum RobotsInputEvent {
    ROBOTS_INPUT_NONE = 0,
    ROBOTS_INPUT_MOVE = 1,
    ROBOTS_INPUT_TELEPORT = 2
} RobotsInputEvent;

/*
 * Cursor (5/6/7/8/0) and Sinclair 1 (6/7/8/9/0) share physical
 * keyboard-matrix contacts.  They cannot be distinguished in software, so
 * exactly one matrix convention must be selected at a time.
 */
typedef enum RobotsMatrixJoystick {
    ROBOTS_MATRIX_JOYSTICK_OFF = 0,
    ROBOTS_MATRIX_JOYSTICK_CURSOR = 1,
    ROBOTS_MATRIX_JOYSTICK_SINCLAIR1 = 2
} RobotsMatrixJoystick;

void robots_input_init(RobotsMatrixJoystick matrix_joystick);
void robots_input_set_matrix_joystick(RobotsMatrixJoystick matrix_joystick);
RobotsMatrixJoystick robots_input_get_matrix_joystick(void);

/*
 * An absent Kempston interface leaves port 0x1f floating on real hardware,
 * so it must be enabled explicitly rather than guessed from port values.
 */
void robots_input_set_kempston(unsigned char enabled);
unsigned char robots_input_get_kempston(void);

/* Require a neutral joystick state before reporting the next event. */
void robots_input_reset(void);

/*
 * Decode an already-normalised z88dk state.  Fire takes precedence over
 * movement; opposite directions cancel independently on each axis.
 */
RobotsInputEvent robots_input_decode_state(unsigned char state,
                                           signed char *dx,
                                           signed char *dy);

/* Pure edge/release filter, exposed so the input contract can be host-tested. */
RobotsInputEvent robots_input_edge_state(unsigned char state,
                                         unsigned char *was_active,
                                         signed char *dx,
                                         signed char *dy);

/* Poll enabled joystick sources together, once per deflection. */
RobotsInputEvent robots_input_poll(signed char *dx, signed char *dy);

/*
 * Non-zero when the last poll saw the selected matrix joystick held.  A
 * caller polling in_inkey() in the same loop should ignore that keyboard
 * result, because matrix joystick contacts appear as number keys.
 */
unsigned char robots_input_matrix_active(void);

#endif
