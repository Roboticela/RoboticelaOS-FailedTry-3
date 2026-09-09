/* robobootloader - animated boot progress engine.
 *
 * Apple-style progress bar behaviour:
 *   - the bar NEVER blocks boot work: boot_progress_pump() advances the
 *     animation by exactly one ~16 ms frame and returns immediately; callers
 *     pump it from their own loops (between read chunks, inside wait stalls)
 *   - the bar moves smoothly on its own schedule: after any jump it glides to
 *     the new value with an ease-out curve (1/6 of the remaining distance per
 *     frame, ~0.5 s per glide), so even instant work looks like calm motion
 *   - long-running work (kernel read) reports byte progress through
 *     boot_progress_span_update(); the bar then tracks the real pace with a
 *     small visual lag, never backwards, never beyond its reserved span
 *   - the module holds no clocks and calls no firmware services, so it is
 *     trivially host-testable; frame pacing (Stall) belongs to boot.c
 *
 * Rendering is decoupled: the engine only holds the displayed per-mille
 * value; the UI observes it with boot_progress_get_displayed() and draws.
 * The timeline always completes at 1000.
 */
#ifndef ROBO_PROGRESS_H
#define ROBO_PROGRESS_H

#include <stdint.h>
#include <stdbool.h>

/* One-time init: reset state. Safe to call again. */
void boot_progress_init(void);

/* Timeline request: glide to `per_mille` (0..1000). Values are floors - the
 * bar never rewinds. 1000 is reached via this (finish path calls it too). */
void boot_progress_set(uint32_t per_mille);

/* Fine-grained progress within a reserved span, e.g. kernel-read bytes:
 * set_span(BOOT_PROG_KERNEL_READ, 450, 600) before starting, then
 * span_update(BOOT_PROG_KERNEL_READ, done, total) per chunk. The effective
 * target becomes span_start + fraction*(span_end-span_start) while the span
 * is armed, but never below the last stage target (monotonic). */
typedef enum {
    BOOT_PROG_KERNEL_READ = 0,
    BOOT_PROG_SPAN_COUNT
} boot_prog_span_t;

void boot_progress_set_span(boot_prog_span_t span, uint32_t start, uint32_t end);
void boot_progress_span_update(boot_prog_span_t span, uint64_t done, uint64_t total);

/* Advance the eased animation by one ~16 ms frame. Cheap and non-blocking:
 * call as often as convenient; pacing (sleep between calls) is the caller's
 * business. */
void boot_progress_pump(void);

/* Request completion (1000) and advance one frame; the UI can then keep
 * pumping until boot_progress_get_displayed() returns 1000. */
void boot_progress_finish(void);

/* Current displayed value (0..1000 per-mille) for the renderer. */
uint32_t boot_progress_get_displayed(void);

#endif /* ROBO_PROGRESS_H */
