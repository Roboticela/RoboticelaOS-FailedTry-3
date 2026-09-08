/* Host unit tests for config parsing (no UEFI involved). */
#include "config/config.h"
#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void)
{
    robo_config_t cfg;
    cfg_result_t r;

    printf("config tests\n");

    /* valid configuration */
    r = config_parse("kernel=RoboBoot/mykernel\n"
                     "logo=RoboBoot/marks.bmp\n"
                     "timeout=7\n"
                     "test=1\n"
                     "debug=1\n", &cfg);
    CHECK(r == CFG_OK, "valid config parses");
    CHECK(robostreq(cfg.kernel_path, "RoboBoot/mykernel"), "kernel path applied");
    CHECK(robostreq(cfg.logo_path, "RoboBoot/marks.bmp"), "logo path applied");
    CHECK(cfg.timeout_seconds == 7, "timeout applied");
    CHECK(cfg.test_mode == true, "test mode on");
    CHECK(cfg.debug == true, "debug on");

    /* empty text -> defaults */
    r = config_parse("", &cfg);
    CHECK(r == CFG_OK, "empty config ok");
    CHECK(robostreq(cfg.kernel_path, "RoboBoot/robokernel"), "defaults restored");

    /* comments + spaces */
    r = config_parse("# comment\n\n  kernel = RoboBoot/k2 \n# x\n", &cfg);
    CHECK(r == CFG_OK, "comments/spaces ok");
    CHECK(robostreq(cfg.kernel_path, "RoboBoot/k2"), "trimmed value");

    /* missing file simulation is handled by the caller (defaults); parse of NULL */
    r = config_parse(NULL, &cfg);
    CHECK(r == CFG_OK, "NULL text -> defaults");

    /* invalid: unknown option */
    r = config_parse("frobnicate=1\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "unknown key rejected");

    /* invalid: no '=' */
    r = config_parse("kernel\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "missing = rejected");

    /* invalid: bad number */
    r = config_parse("timeout=abc\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "bad timeout rejected");

    /* invalid: giant timeout */
    r = config_parse("timeout=99999\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "oversized timeout rejected");

    /* invalid: absolute path */
    r = config_parse("kernel=/etc/passwd\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "absolute path rejected");

    /* invalid: backslash path */
    r = config_parse("kernel=RoboBoot\\k\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "backslash path rejected");

    /* invalid: traversal */
    r = config_parse("kernel=../secret\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "traversal rejected");

    /* invalid: empty key */
    r = config_parse("=value\n", &cfg);
    CHECK(r == CFG_ERR_SYNTAX, "empty key rejected");

    /* too long value */
    {
        char longline[600];
        memset(longline, 'a', sizeof(longline));
        memcpy(longline, "kernel=", 7);
        longline[sizeof(longline) - 1] = '\0';
        r = config_parse(longline, &cfg);
        CHECK(r == CFG_ERR_TOO_LONG || r == CFG_ERR_SYNTAX, "long value rejected");
    }

    if (failures) {
        printf("%d FAILURES\n", failures);
        return 1;
    }
    printf("all config tests passed\n");
    return 0;
}
