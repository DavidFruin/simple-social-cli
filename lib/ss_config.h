#ifndef SS_CONFIG_H
#define SS_CONFIG_H

#define CFG_MAX_URL 256
#define CFG_MAX_PATH 512

typedef struct {
    char base_url[CFG_MAX_URL];
    char data_dir[CFG_MAX_PATH];
    char download_dir[CFG_MAX_PATH];
} config_t;

int config_load(config_t *cfg);
const char *config_get_base_url(void);

#endif
