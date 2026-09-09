/* Host unit tests for the animated boot progress engine (no UEFI involved).
 *
 * Semantics under test: the displayed value is a continuous ease-out sweep
 * (remaining/6 per pump) toward the effective target, so it lags real work by
 * a few frames but never rewinds and always completes at 1000.
 */
#include "graphics/progress.h"

#include <stdio.h>

static int failures = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            printf("  ok  %s\n", msg);                                   \
        } else {                                                          \
            printf("FAIL  %s (line %d)\n", msg, __LINE__);               \
            failures++;                                                   \
        }                                                                 \
    } while (0)

/* Pump until the value stops changing (bounded). Returns frames used. */
static int settle(void)
{
    uint32_t prev = boot_progress_get_displayed();
    int frames = 0;
    while (frames < 10000) {
        boot_progress_pump();
        frames++;
        uint32_t cur = boot_progress_get_displayed();
        if (cur == prev)
            break;
        prev = cur;
    }
    return frames;
}

int main(void)
{
    printf("progress engine tests\n");

    /* animation: a jump is never instant */
    boot_progress_init();
    boot_progress_set(1000);
    boot_progress_pump();
    CHECK(boot_progress_get_displayed() < 1000, "1000 is not instant (animated)");
    {
        int f = settle();
        CHECK(f >= 30 && f <= 120, "full 0->1000 sweep takes ~0.5-2 s of frames");
    }

    /* smooth: first frame of a full sweep moves a sane fraction */
    boot_progress_init();
    boot_progress_set(1000);
    boot_progress_pump();
    {
        uint32_t v = boot_progress_get_displayed();
        CHECK(v >= 100 && v <= 300, "first frame covers ~1/6 of the distance");
    }

    /* small targets complete exactly */
    boot_progress_init();
    boot_progress_set(3);
    settle();
    CHECK(boot_progress_get_displayed() == 3, "tiny target reached exactly");

    /* monotonic: a lower stage target never rewinds the bar */
    boot_progress_init();
    boot_progress_set(500);
    settle();
    {
        uint32_t v = boot_progress_get_displayed();
        CHECK(v == 500, "reaches 500");
        boot_progress_set(100);
        settle();
        CHECK(boot_progress_get_displayed() == 500, "lower target ignored");
    }

    /* byte-driven span sweeps through its reserved bounds */
    boot_progress_init();
    boot_progress_set_span(BOOT_PROG_KERNEL_READ, 450, 600);
    boot_progress_span_update(BOOT_PROG_KERNEL_READ, 0, 1000000);
    settle();
    CHECK(boot_progress_get_displayed() == 450, "empty span settles at span_start");
    boot_progress_span_update(BOOT_PROG_KERNEL_READ, 500000, 1000000);
    settle();
    CHECK(boot_progress_get_displayed() == 525, "half read settles mid-span");
    boot_progress_span_update(BOOT_PROG_KERNEL_READ, 1000000, 1000000);
    settle();
    CHECK(boot_progress_get_displayed() == 600, "full read settles at span_end");

    /* real pace: value tracks updates monotonically between chunks */
    boot_progress_init();
    boot_progress_set_span(BOOT_PROG_KERNEL_READ, 450, 600);
    {
        uint32_t last = 0;
        bool monotonic = true;
        uint64_t i;
        for (i = 1; i <= 10; i++) {
            boot_progress_span_update(BOOT_PROG_KERNEL_READ, i * 100000, 1000000);
            boot_progress_pump();
            uint32_t v = boot_progress_get_displayed();
            if (v < last)
                monotonic = false;
            last = v;
        }
        CHECK(monotonic, "chunked updates never move the bar backwards");
    }

    /* finish completes to exactly 1000 */
    boot_progress_init();
    boot_progress_set(200);
    settle();
    boot_progress_finish();
    settle();
    CHECK(boot_progress_get_displayed() == 1000, "finish reaches 1000");

    /* finish is safe from a fresh state */
    boot_progress_init();
    boot_progress_finish();
    settle();
    CHECK(boot_progress_get_displayed() == 1000, "finish from zero completes");

    /* pump loop always terminates */
    boot_progress_init();
    boot_progress_set(1000);
    {
        int f = settle();
        CHECK(f < 10000, "pump loop terminates");
    }

    if (failures) {
        printf("%d FAILURES\n", failures);
        return 1;
    }
    printf("all progress tests passed\n");
    return 0;
}
