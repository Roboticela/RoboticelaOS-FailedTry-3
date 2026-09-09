/* robobootloader - firmware keyboard input.
 *
 * Reads the UEFI console keyboard through the ConsoleIn Simple Text Input
 * protocol, preferring the UEFI 2.x Simple Text Input EX protocol when the
 * firmware provides it: EX reports the modifier state (Shift/Ctrl/Alt/Logo),
 * which is how "hold Alt while powering on" is detected - plain ConIn only
 * delivers translated keystrokes and hides modifiers on non-printable keys.
 *
 * Freestanding; only boot services are used, never after ExitBootServices.
 */
#ifndef ROBO_KEYBOARD_H
#define ROBO_KEYBOARD_H

#include "uefi/uefi.h"

#include <stdint.h>
#include <stdbool.h>

/* A decoded keystroke. */
typedef struct {
    uint16_t scan_code;         /* SCAN_* (0 for plain characters) */
    CHAR16   unicode;           /* UCS-2 char (0 for non-printables) */
    bool     alt_down;          /* Alt held (left or right) */
    bool     shift_down;
    bool     ctrl_down;
} robo_key_t;

/* Locate the console input protocols. Call once at boot. Safe to call when
 * no keyboard exists (all functions then behave as "nothing pressed"). */
void keyboard_init(void);

/* Try to read one keystroke. Returns false when the queue is empty. */
bool keyboard_poll(robo_key_t *key);

/* Modifier state snapshot (no key required). EX firmware only; on plain
 * ConIn firmware this can only see modifiers via keystrokes, so it polls
 * (draining nothing when the queue is empty). Returns true if Alt is held. */
bool keyboard_alt_down(void);

/* Drain any pending keystrokes (used when leaving the menu). */
void keyboard_flush(void);

#endif /* ROBO_KEYBOARD_H */
