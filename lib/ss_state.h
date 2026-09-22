#ifndef SS_STATE_H
#define SS_STATE_H

#define STATE_MAX_JWT 1024
#define STATE_MAX_EMAIL 256

typedef struct {
    int user_id;
    char email[STATE_MAX_EMAIL];
    char created_at[32];
} user_t;

typedef struct {
    char jwt[STATE_MAX_JWT];
    char refresh[STATE_MAX_JWT];
    user_t user;
} ss_state_t;

// Names the tool whose session this is, deciding which subdirectory of
// ~/.simple-social-cli the tokens live in. Each front end sets its own, so
// each holds an independent server-side session and appears separately in
// the account's device list. Call before any load/save; defaults to "cli".
void ss_state_set_app(const char *app);

void ss_state_init(ss_state_t *state);
int ss_state_is_logged_in(ss_state_t *state);
void ss_state_set_jwt(ss_state_t *state, const char *jwt);
void ss_state_set_refresh(ss_state_t *state, const char *refresh);
void ss_state_set_user(ss_state_t *state, int id, const char *email, const char *created_at);
void ss_state_clear(ss_state_t *state);
int ss_state_save_jwt(ss_state_t *state);
int ss_state_load_jwt(ss_state_t *state);
int ss_state_save_refresh(ss_state_t *state);
int ss_state_load_refresh(ss_state_t *state);
int ss_state_save_user(ss_state_t *state);
int ss_state_load_user(ss_state_t *state);
// Removes this tool's stored tokens (used by logout and on a dead session).
void ss_state_delete_files(void);

#endif
