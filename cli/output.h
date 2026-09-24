#ifndef OUTPUT_H
#define OUTPUT_H

#include "ss_api.h"

extern int g_color_enabled;
extern int g_json_enabled;
extern const char *C_RESET;
extern const char *C_BOLD;
extern const char *C_CYAN;
extern const char *C_GREEN;
extern const char *C_YELLOW;
extern const char *C_RED;
extern const char *C_DIM;

void output_init(void);
void print_posts(const api_posts_result_t *result);
void print_post(const api_post_t *post);
void print_users(const api_users_result_t *result);
void print_likes(const api_users_result_t *result);
void print_user_info(int id, const char *email, const char *created_at);
void print_comments(const api_comments_result_t *result);
void print_notifications(const api_notifications_result_t *result);
void print_profile(int user_id, const char *email, const char *created_at,
                   int follower_count, int following_count);
void print_success(const char *msg);
void print_error(const char *msg);
void print_json_error(const char *msg);
void print_count(int count);
void print_count_json(int count, const char *key);
#endif
