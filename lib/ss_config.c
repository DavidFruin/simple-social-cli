#include "ss_config.h"
#include "ss_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static config_t g_config = {
    .base_url = "https://dev.davidfruin.com/api.php",
    .data_dir = "",
    .download_dir = ""
};

static void ensure_dirs(const char *home) {
    char path[CFG_MAX_PATH];
    snprintf(path, sizeof(path), "%s/.simple-social-cli", home);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/.config/simple-social-cli", home);
    mkdir(path, 0755);
}

static void set_defaults(const char *home) {
    snprintf(g_config.data_dir, CFG_MAX_PATH, "%s/.simple-social-cli", home);
    snprintf(g_config.download_dir, CFG_MAX_PATH, "%s/Downloads", home);
}

int config_load(config_t *cfg) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    ensure_dirs(home);
    set_defaults(home);

    char path[CFG_MAX_PATH];
    snprintf(path, sizeof(path), "%s/.config/simple-social-cli/config.ini", home);
    FILE *f = fopen(path, "r");
    if (!f) {
        *cfg = g_config;
        return 0;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *s = str_trim(line);
        if (*s == '#' || *s == '\0') continue;
        if (*s == '[') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = str_trim(s);
        char *val = str_trim(eq + 1);
        if (strcmp(key, "base_url") == 0) {
            strncpy(g_config.base_url, val, CFG_MAX_URL - 1);
        } else if (strcmp(key, "download_dir") == 0) {
            strncpy(g_config.download_dir, val, CFG_MAX_PATH - 1);
        }
    }
    fclose(f);
    *cfg = g_config;
    return 0;
}

const char *config_get_base_url(void) {
    return g_config.base_url;
}
