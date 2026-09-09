/* robobootloader - boot.cfg parser.
 *
 * Minimal line-based format:
 *     key=value
 * '#' starts a comment. Blank lines ignored. Unknown keys are errors (fail
 * safe). All strings are copied into fixed-size storage; long values are
 * rejected, not truncated. Paths must be relative, backslash-free, and may
 * not contain ".." segments.
 */
#ifndef ROBO_CONFIG_H
#define ROBO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define ROBO_CFG_MAX_PATH 256

/* When to show the Apple-style boot device picker (boot.cfg "menu="). */
typedef enum {
    ROBO_MENU_ALT = 0,      /* default: picker when Alt (or F2/Down) is held */
    ROBO_MENU_ALWAYS,       /* picker on every boot */
    ROBO_MENU_NONE          /* never show the picker */
} robo_menu_mode_t;

typedef struct {
    char kernel_path[ROBO_CFG_MAX_PATH];
    char logo_path[ROBO_CFG_MAX_PATH];
    char initfs_path[ROBO_CFG_MAX_PATH];   /* optional; empty = not requested */
    uint32_t timeout_seconds;              /* picker countdown, seconds */
    robo_menu_mode_t menu_mode;            /* see robo_menu_mode_t */
    bool test_mode;                        /* test=1: hold boot screen for inspection */
    bool debug;                            /* debug=1: serial diagnostics */
} robo_config_t;

typedef enum {
    CFG_OK = 0,
    CFG_ERR_NOT_FOUND,      /* file missing -> caller uses compiled-in defaults */
    CFG_ERR_READ,
    CFG_ERR_SYNTAX,         /* malformed line / unknown key / bad value */
    CFG_ERR_TOO_LONG
} cfg_result_t;

/* Parse configuration text (NUL-terminated). Fills *cfg with defaults first,
 * then applies overrides. Returns CFG_OK or a specific error. */
cfg_result_t config_parse(const char *text, robo_config_t *cfg);

/* Compiled-in defaults (also the fallback when boot.cfg is missing). */
void config_defaults(robo_config_t *cfg);

#endif /* ROBO_CONFIG_H */
