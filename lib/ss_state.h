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
    user_t user;
} ss_state_t;

void ss_state_init(ss_state_t *state);
int ss_state_is_logged_in(ss_state_t *state);
void ss_state_set_jwt(ss_state_t *state, const char *jwt);
void ss_state_set_user(ss_state_t *state, int id, const char *email, const char *created_at);
void ss_state_clear(ss_state_t *state);
int ss_state_save_jwt(ss_state_t *state);
int ss_state_load_jwt(ss_state_t *state);
int ss_state_save_user(ss_state_t *state);
int ss_state_load_user(ss_state_t *state);

#endif
