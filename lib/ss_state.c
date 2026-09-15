#include "ss_state.h"
#include "ss_json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void ss_state_init(ss_state_t *state) {
    memset(state, 0, sizeof(*state));
}

int ss_state_is_logged_in(ss_state_t *state) {
    return state->jwt[0] != '\0';
}

void ss_state_set_jwt(ss_state_t *state, const char *jwt) {
    strncpy(state->jwt, jwt, STATE_MAX_JWT - 1);
    state->jwt[STATE_MAX_JWT - 1] = '\0';
}

void ss_state_set_user(ss_state_t *state, int id, const char *email, const char *created_at) {
    state->user.user_id = id;
    if (email) strncpy(state->user.email, email, STATE_MAX_EMAIL - 1);
    if (created_at) strncpy(state->user.created_at, created_at, sizeof(state->user.created_at) - 1);
}

void ss_state_clear(ss_state_t *state) {
    memset(state->jwt, 0, STATE_MAX_JWT);
    memset(&state->user, 0, sizeof(user_t));
}

static const char *get_data_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    static char path[512];
    snprintf(path, sizeof(path), "%s/.simple-social-cli", home);
    return path;
}

static const char *get_legacy_path(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    static char lpath[512];
    snprintf(lpath, sizeof(lpath), "%s/.simple-social-tui", home);
    return lpath;
}

static void ensure_data_dir(void) {
    mkdir(get_data_path(), 0755);
}

int ss_state_save_jwt(ss_state_t *state) {
    ensure_data_dir();
    char path[512];
    snprintf(path, sizeof(path), "%s/jwt.txt", get_data_path());
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", state->jwt);
    fclose(f);
    return 0;
}

int ss_state_load_jwt(ss_state_t *state) {
    char path[512];
    snprintf(path, sizeof(path), "%s/jwt.txt", get_data_path());
    FILE *f = fopen(path, "r");
    if (!f) {
        char lpath[512]; snprintf(lpath, sizeof(lpath), "%s/jwt.txt", get_legacy_path());
        f = fopen(lpath, "r");
        if (!f) return -1;
    }
    char buf[STATE_MAX_JWT];
    if (fgets(buf, sizeof(buf), f)) {
        char *end = buf + strlen(buf) - 1;
        while (end >= buf && (*end == '\n' || *end == '\r')) *end-- = '\0';
        ss_state_set_jwt(state, buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    return -1;
}

int ss_state_save_user(ss_state_t *state) {
    ensure_data_dir();
    char path[512];
    snprintf(path, sizeof(path), "%s/user.json", get_data_path());
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\"id\":%d,\"email\":\"%s\",\"created_at\":\"%s\"}\n",
            state->user.user_id, state->user.email, state->user.created_at);
    fclose(f);
    return 0;
}

int ss_state_load_user(ss_state_t *state) {
    char path[512];
    snprintf(path, sizeof(path), "%s/user.json", get_data_path());
    FILE *f = fopen(path, "r");
    if (!f) {
        char lpath[512]; snprintf(lpath, sizeof(lpath), "%s/user.json", get_legacy_path());
        f = fopen(lpath, "r");
        if (!f) return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    int id = 0;
    char email[STATE_MAX_EMAIL] = {0};
    char created[32] = {0};
    char *ep = strstr(buf, "\"id\":");
    if (ep) id = atoi(ep + 5);
    if (json_get_string(buf, "email", email, sizeof(email)) != 0) {
        free(buf);
        return -1;
    }
    json_get_string(buf, "created_at", created, sizeof(created));
    ss_state_set_user(state, id, email, created);
    free(buf);
    return 0;
}
