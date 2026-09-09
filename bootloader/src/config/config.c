/* robobootloader - boot.cfg parser implementation. */
#include "config.h"
#include "lib.h"

void config_defaults(robo_config_t *cfg)
{
    robomemset(cfg, 0, sizeof(*cfg));
    robostrncpy(cfg->kernel_path, sizeof(cfg->kernel_path), "RoboBoot/robokernel");
    robostrncpy(cfg->logo_path, sizeof(cfg->logo_path), "RoboBoot/logo.bmp");
    robostrncpy(cfg->initfs_path, sizeof(cfg->initfs_path), "RoboBoot/robo-initfs");
    cfg->timeout_seconds = 3;
    cfg->menu_mode = ROBO_MENU_ALT;
    cfg->test_mode = false;
    cfg->debug = false;
}

/* A usable path: non-empty, shorter than the buffer, no backslashes, no
 * leading '/', no ".." segments, no drive letters. */
static bool path_ok(const char *p)
{
    if (!p || !*p)
        return false;
    if (p[1] == ':' ) /* C:\ style */
        return false;
    for (const char *q = p; *q; q++) {
        char c = *q;
        if (c == '\\' || c == '\r' || c == '\n' || c < 0x20 || c == 0x7F)
            return false;
    }
    if (*p == '/')
        return false;
    if (robolen(p) >= ROBO_CFG_MAX_PATH)
        return false;
    /* reject any ".." segment */
    {
        const char *q = p;
        while (*q) {
            if (q[0] == '.' && q[1] == '.' &&
                (q == p || q[-1] == '/') &&
                (q[2] == '\0' || q[2] == '/'))
                return false;
            q++;
        }
    }
    return true;
}

static cfg_result_t apply_kv(robo_config_t *cfg, const char *key, const char *val)
{
    if (robostreq(key, "kernel")) {
        if (!path_ok(val))
            return CFG_ERR_SYNTAX;
        robostrncpy(cfg->kernel_path, sizeof(cfg->kernel_path), val);
        return CFG_OK;
    }
    if (robostreq(key, "logo")) {
        if (!path_ok(val))
            return CFG_ERR_SYNTAX;
        robostrncpy(cfg->logo_path, sizeof(cfg->logo_path), val);
        return CFG_OK;
    }
    if (robostreq(key, "initfs")) {
        if (val[0] == '\0') {
            cfg->initfs_path[0] = '\0'; /* explicitly disabled */
            return CFG_OK;
        }
        if (!path_ok(val))
            return CFG_ERR_SYNTAX;
        robostrncpy(cfg->initfs_path, sizeof(cfg->initfs_path), val);
        return CFG_OK;
    }
    if (robostreq(key, "timeout")) {
        uint64_t v;
        if (!roboparse_u64(val, &v) || v > 3600)
            return CFG_ERR_SYNTAX;
        cfg->timeout_seconds = (uint32_t)v;
        return CFG_OK;
    }
    if (robostreq(key, "menu")) {
        if (robostreq(val, "alt")) {
            cfg->menu_mode = ROBO_MENU_ALT;
            return CFG_OK;
        }
        if (robostreq(val, "always")) {
            cfg->menu_mode = ROBO_MENU_ALWAYS;
            return CFG_OK;
        }
        if (robostreq(val, "none")) {
            cfg->menu_mode = ROBO_MENU_NONE;
            return CFG_OK;
        }
        return CFG_ERR_SYNTAX;
    }
    if (robostreq(key, "test")) {
        if (robostreq(val, "1")) {
            cfg->test_mode = true;
            return CFG_OK;
        }
        if (robostreq(val, "0")) {
            cfg->test_mode = false;
            return CFG_OK;
        }
        return CFG_ERR_SYNTAX;
    }
    if (robostreq(key, "debug")) {
        if (robostreq(val, "1")) {
            cfg->debug = true;
            return CFG_OK;
        }
        if (robostreq(val, "0")) {
            cfg->debug = false;
            return CFG_OK;
        }
        return CFG_ERR_SYNTAX;
    }
    return CFG_ERR_SYNTAX; /* unknown key: fail safe */
}

cfg_result_t config_parse(const char *text, robo_config_t *cfg)
{
    config_defaults(cfg);
    if (!text)
        return CFG_OK; /* defaults only */

    const char *p = text;
    while (*p) {
        char line[512];
        uint64_t n = 0;

        /* copy one line */
        while (*p && *p != '\n' && *p != '\r') {
            if (n >= sizeof(line) - 1)
                return CFG_ERR_TOO_LONG;
            line[n++] = *p++;
        }
        /* skip EOL */
        if (*p == '\r')
            p++;
        if (*p == '\n')
            p++;
        line[n] = '\0';

        /* strip comment */
        {
            char *hash = line;
            while (*hash && *hash != '#')
                hash++;
            *hash = '\0';
        }

        /* trim leading spaces */
        {
            char *s = line;
            while (*s == ' ' || *s == '\t')
                s++;
            if (!*s)
                continue; /* blank */

            /* split at '=' */
            char *eq = s;
            while (*eq && *eq != '=')
                eq++;
            if (!*eq)
                return CFG_ERR_SYNTAX;
            *eq = '\0';
            char *key = s;
            char *val = eq + 1;

            /* trim trailing spaces of key, leading of val */
            {
                char *e = key + robolen(key);
                while (e > key && (e[-1] == ' ' || e[-1] == '\t'))
                    *--e = '\0';
            }
            while (*val == ' ' || *val == '\t')
                val++;
            {
                char *e = val + robolen(val);
                while (e > val && (e[-1] == ' ' || e[-1] == '\t'))
                    *--e = '\0';
            }
            if (!*key)
                return CFG_ERR_SYNTAX;

            {
                cfg_result_t r = apply_kv(cfg, key, val);
                if (r != CFG_OK)
                    return r;
            }
        }
    }
    return CFG_OK;
}
