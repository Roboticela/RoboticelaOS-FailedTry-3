/* robobootloader - firmware keyboard input implementation. */
#include "keyboard.h"
#include "uefi/uefi.h"
#include "memory/memory.h"
#include "debug.h"
#include "lib.h"

#include <stdint.h>
#include <stdbool.h>

static EFI_SIMPLE_TEXT_INPUT_PROTOCOL *g_conin;
static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *g_ex;

void keyboard_init(void)
{
    EFI_SYSTEM_TABLE *syst = st();

    g_conin = NULL;
    g_ex = NULL;
    if (!syst || !syst->ConIn)
        return;
    g_conin = syst->ConIn;

    /* Optional EX protocol: enables modifier detection (Alt, etc.). */
    g_ex = console_input_ex();
    debugf("keyboard: ConIn %s, EX %s",
           g_conin ? "yes" : "no", g_ex ? "yes" : "no");
}

static void decode_key(EFI_INPUT_KEY *k, UINT32 shift, robo_key_t *out)
{
    out->scan_code = k->ScanCode;
    out->unicode = k->UnicodeChar;
    out->alt_down = (shift & (EFI_LEFT_ALT_PRESSED | EFI_RIGHT_ALT_PRESSED)) != 0;
    out->shift_down = (shift & (EFI_LEFT_SHIFT_PRESSED | EFI_RIGHT_SHIFT_PRESSED)) != 0;
    out->ctrl_down = (shift & (EFI_LEFT_CONTROL_PRESSED | EFI_RIGHT_CONTROL_PRESSED)) != 0;
}

bool keyboard_poll(robo_key_t *key)
{
    EFI_STATUS s;

    if (!key)
        return false;

    if (g_ex) {
        EFI_KEY_DATA kd;
        robomemset(&kd, 0, sizeof(kd));
        s = g_ex->ReadKeyStrokeEx(g_ex, &kd);
        if (EFI_ERROR(s))
            return false;
        if (!(kd.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID))
            kd.KeyState.KeyShiftState = EFI_SHIFT_STATE_VALID;
        decode_key(&kd.Key, kd.KeyState.KeyShiftState, key);
        return true;
    }

    if (g_conin) {
        EFI_INPUT_KEY k;
        s = g_conin->ReadKeyStroke(g_conin, &k);
        if (EFI_ERROR(s))
            return false;
        decode_key(&k, EFI_SHIFT_STATE_VALID, key);
        return true;
    }
    return false;
}

/* Alt held right now? Only the EX protocol can truly answer this; plain
 * ConIn firmware sees modifiers only inside keystrokes. */
bool keyboard_alt_down(void)
{
    if (g_ex) {
        /* ReadKeyStrokeEx returns EFI_NOT_READY when the queue is empty and
         * drains nothing; poll drains nothing when the queue is empty, so
         * this is a cheap state snapshot attempt. */
        EFI_KEY_DATA kd;
        if (!g_ex)
            return false;
        robomemset(&kd, 0, sizeof(kd));
        if (EFI_ERROR(g_ex->ReadKeyStrokeEx(g_ex, &kd)))
            return false;
        if (!(kd.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID))
            return false;
        return (kd.KeyState.KeyShiftState &
                (EFI_LEFT_ALT_PRESSED | EFI_RIGHT_ALT_PRESSED)) != 0;
    }
    return false;
}

void keyboard_flush(void)
{
    robo_key_t k;
    while (keyboard_poll(&k))
        ;
}
