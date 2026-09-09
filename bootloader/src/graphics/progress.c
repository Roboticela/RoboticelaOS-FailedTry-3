/* robobootloader - animated boot progress engine implementation.
 *
 * Motion model: every pump() advances the displayed value by 1/6 of the
 * remaining distance (one ~16 ms frame), so a jump of any size glides in over
 * roughly half a second with an ease-out curve, and byte-driven targets track
 * real work with ~100 ms of visual lag. Monotonic: the bar never moves
 * backwards. Nothing here blocks or sleeps: callers pump from their own
 * loops; frame pacing lives in boot.c.
 */
#include "progress.h"

#include <stdint.h>

#define PROG_EASE_NUM        1u     /* step = remaining * 1/6 per frame */
#define PROG_EASE_DEN        6u

typedef struct {
    uint32_t target;            /* stage/finish target (per-mille) */
    uint32_t displayed;         /* eased value shown on screen */
    uint32_t span_start, span_end;
    uint64_t span_done, span_total;
    bool     span_armed;
} prog_state_t;

static prog_state_t g_prog;
static bool g_inited;

void boot_progress_init(void)
{
    g_prog.target = 0;
    g_prog.displayed = 0;
    g_prog.span_start = 0;
    g_prog.span_end = 1000;
    g_prog.span_done = 0;
    g_prog.span_total = 0;
    g_prog.span_armed = false;
    g_inited = true;
}

/* Effective target: while an armed span has byte data, it is the byte
 * fraction's position inside the span - but never below the stage target. */
static uint32_t effective_target(void)
{
    prog_state_t *s = &g_prog;
    if (s->span_armed && s->span_total > 0 && s->span_end > s->span_start) {
        uint64_t done = s->span_done, total = s->span_total;
        if (done > total)
            done = total;
        {
            uint64_t v = s->span_start +
                         ((done * 1000u / total) * (s->span_end - s->span_start)) / 1000u;
            if (v > s->target)
                return (uint32_t)v;
        }
    }
    return s->target;
}

void boot_progress_set_span(boot_prog_span_t span, uint32_t start, uint32_t end)
{
    (void)span;
    if (end < start)
        end = start;
    g_prog.span_start = start;
    g_prog.span_end = end;
    g_prog.span_done = 0;
    g_prog.span_total = 0;
    g_prog.span_armed = true;
}

void boot_progress_span_update(boot_prog_span_t span, uint64_t done, uint64_t total)
{
    (void)span;
    if (total == 0)
        return;
    if (done > total)
        done = total;
    g_prog.span_done = done;
    g_prog.span_total = total;
}

void boot_progress_set(uint32_t per_mille)
{
    if (per_mille > 1000)
        per_mille = 1000;
    if (per_mille < g_prog.target)
        return;                         /* monotonic */
    g_prog.target = per_mille;
}

void boot_progress_pump(void)
{
    prog_state_t *s = &g_prog;
    uint32_t target, remaining;
    uint64_t step;

    if (!g_inited)
        boot_progress_init();

    target = effective_target();
    if (s->displayed >= target)
        return;

    remaining = target - s->displayed;
    step = ((uint64_t)remaining * PROG_EASE_NUM) / PROG_EASE_DEN;
    if (step == 0)
        step = 1;                       /* always crawl forward */
    s->displayed += (uint32_t)step;
    if (s->displayed > target)
        s->displayed = target;
}

void boot_progress_finish(void)
{
    if (!g_inited)
        boot_progress_init();
    g_prog.span_armed = false;          /* byte spans end at completion */
    g_prog.span_total = 0;
    boot_progress_set(1000);
    boot_progress_pump();
}

uint32_t boot_progress_get_displayed(void)
{
    return g_inited ? g_prog.displayed : 0;
}
