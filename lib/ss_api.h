#ifndef SS_API_H
#define SS_API_H

#define API_MAX_RESPONSE 65536
#define API_MAX_POSTS 256
#define API_MAX_LINE 512

typedef struct {
    int id;
    char email[256];
    char created_at[32];
} api_user_t;

typedef struct {
    char id[64];
    char text[5000];
    char timestamp[32];
    int like_count;
    int is_liked;
    char media_url[512];
    int user_id;
    char user_email[256];
} api_post_t;

typedef struct {
    int id;
    int post_id;
    int user_id;
    char text[5000];
    char created_at[32];
    char user_email[256];
} api_comment_t;

typedef struct {
    int id;
    int recipient_id;
    int actor_id;
    char actor_email[256];
    char type[32];
    char post_id[64];
    char created_at[32];
} api_notification_t;

typedef struct {
    api_post_t posts[API_MAX_POSTS];
    int count;
    int has_more;
    int total_count;
} api_posts_result_t;

typedef struct {
    api_comment_t comments[256];
    int count;
    int has_more;
    int total_count;
} api_comments_result_t;

typedef struct {
    api_user_t users[256];
    int count;
} api_users_result_t;

typedef struct {
    api_notification_t notifications[256];
    int count;
} api_notifications_result_t;

int api_init(void);
void api_cleanup(void);
void api_set_jwt(const char *jwt);
void api_set_user_id(int user_id);
int api_call(const char *action, const char *params, char *response, int resp_size);
const char *api_get_last_error(void);

// Identifies this tool to the server, which parses it into the device name
// shown in the account's device list. Set once at startup.
void api_set_user_agent(const char *ua);

// ---- Sessions ----------------------------------------------------------
// An access token is short-lived; the refresh token is what keeps this tool
// signed in. Hand the stored one back after loading it from disk, and
// api_call() will use it to renew silently instead of failing on a token
// that merely aged out.
void api_set_refresh_token(const char *refresh);
// The refresh token issued by the most recent api_login(), for saving.
const char *api_get_refresh_token(void);
// Exchanges the refresh token for a new access token. Returns 0 on success.
// Called automatically by api_call() on a 401; rarely needed directly.
int api_refresh_session(void);
// Invoked whenever a refresh produces a new access token, so the caller can
// persist it - the API layer deliberately knows nothing about storage.
void api_set_token_refreshed_cb(void (*cb)(const char *jwt));

int api_login(const char *email, const char *password, char *jwt_out, int jwt_size, int *user_id_out);
int api_logout(void);
int api_get_my_info(int *id_out, char *email_out, int email_size, char *created_out, int created_size);
int api_get_user_info(int user_id, char *email_out, int email_size, char *created_out, int created_size);
int api_get_users(api_users_result_t *result);
int api_get_user_emails(int *user_ids, int count, char *out, int out_size);
int api_register_send_otp(const char *email);
int api_register_verify_otp(const char *email, const char *otp);
int api_register_finish(const char *email, const char *password, const char *confirm);
int api_send_otp(const char *email);
int api_verify_otp(const char *email, const char *otp);
int api_reset_password(const char *email, const char *password, const char *confirm);
int api_create_post(const char *text, const char *media_url, char *post_id_out, int post_id_size);
int api_delete_post(const char *post_id);
int api_get_my_posts(int offset, int limit, api_posts_result_t *result);
int api_get_user_posts(int user_id, int offset, int limit, api_posts_result_t *result);
int api_fetch_followed_posts(int offset, int limit, api_posts_result_t *result);
int api_like_post(const char *post_id);
int api_unlike_post(const char *post_id);
int api_get_post_likes(const char *post_id, char *out, int out_size);
int api_follow_user(int user_id);
int api_unfollow_user(int user_id);
int api_is_following(int user_id, int *out);
int api_get_my_follows(int user_id, api_users_result_t *result);
int api_get_my_followers(int user_id, api_users_result_t *result);
int api_create_comment(const char *post_id, const char *text, int *comment_id_out);
int api_get_post_comments(const char *post_id, int offset, int limit, api_comments_result_t *result);
int api_delete_comment(int comment_id);
int api_get_post_comment_counts(char post_ids[][64], int count, char *out, int out_size);
int api_get_notifications(int offset, api_notifications_result_t *result);
int api_get_unseen_notification_count(int *count_out);
int api_mark_notifications_seen(void);
int api_get_post_by_id(const char *post_id, api_post_t *post_out);
int api_delete_account(const char *password);
int api_upload_media(const char *filepath, char *url_out, int url_size);
int api_delete_media(int media_id);
int api_upload_media_with_id(const char *filepath, char *url_out, int url_size, int *id_out);

#endif
