#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ss_api.h"
#include "ss_config.h"
#include "ss_state.h"
#include "output.h"
#include "ss_json.h"
#include <ctype.h>

static ss_state_t state;

static void auto_login(void) {
    config_t cfg;
    config_load(&cfg);
    api_init();

    if (ss_state_load_jwt(&state) == 0) {
        api_set_jwt(state.jwt);
        int id = 0;
        char email[256] = {0};
        char created[32] = {0};
        if (api_get_my_info(&id, email, sizeof(email), created, sizeof(created)) == 0) {
            ss_state_set_user(&state, id, email, created);
            api_set_user_id(id);
            return;
        }
        ss_state_clear(&state);
    }
}

static int require_auth(void) {
    if (!ss_state_is_logged_in(&state)) {
        print_error("Not logged in. Run: simple-social-cli login <email> <password>");
        return -1;
    }
    return 0;
}

static int cmd_login(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: simple-social-cli login <email> <password>\n"); return 1; }
    const char *email = argv[0];
    const char *password = argv[1];

    config_t cfg;
    config_load(&cfg);
    api_init();

    char jwt[1024] = {0};
    int user_id = 0;
    if (api_login(email, password, jwt, sizeof(jwt), &user_id) != 0) {
        print_error(api_get_last_error());
        return 1;
    }

    ss_state_set_jwt(&state, jwt);
    api_set_jwt(jwt);
    api_set_user_id(user_id);
    ss_state_save_jwt(&state);

    char email_out[256] = {0};
    char created[32] = {0};
    api_get_my_info(&user_id, email_out, sizeof(email_out), created, sizeof(created));
    ss_state_set_user(&state, user_id, email_out, created);
    ss_state_save_user(&state);

    if (g_json_enabled) printf("{\"ok\":true,\"userId\":%d,\"email\":\"%s\"}\n", user_id, email_out); else fprintf(stderr, "Logged in as %s (ID: %d)\n", email_out, user_id);
    return 0;
}

static int cmd_logout(int argc, char **argv) {
    (void)argc; (void)argv;
    ss_state_clear(&state);
    char path[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.simple-social-cli/jwt.txt", home);
    unlink(path);
    snprintf(path, sizeof(path), "%s/.simple-social-cli/user.json", home);
    unlink(path);
    snprintf(path, sizeof(path), "%s/.simple-social-tui/jwt.txt", home);
    unlink(path);
    snprintf(path, sizeof(path), "%s/.simple-social-tui/user.json", home);
    unlink(path);
    if (g_json_enabled) printf("{\"ok\":true}\n"); else fprintf(stderr, "Logged out.\n");
    return 0;
}

static int cmd_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    if (require_auth() != 0) return 1;
    print_user_info(state.user.user_id, state.user.email, state.user.created_at);
    return 0;
}

static int cmd_register(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: simple-social-cli register <email> <otp> <password> <confirm>\n"); return 1; }
    const char *email = argv[0];
    const char *otp = argv[1];
    const char *password = argv[2];
    const char *confirm = argv[3];

    config_t cfg;
    config_load(&cfg);
    api_init();

    if (api_register_verify_otp(email, otp) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (api_register_finish(email, password, confirm) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"ok\":true}\n"); else fprintf(stderr, "Registration complete. You can now login.\n");
    return 0;
}

static int cmd_send_otp(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli send-otp <email>\n"); return 1; }
    config_t cfg;
    config_load(&cfg);
    api_init();
    if (api_register_send_otp(argv[0]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"ok\":true}\n"); else fprintf(stderr, "OTP sent to %s\n", argv[0]);
    return 0;
}

static int cmd_reset_password(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "Usage: simple-social-cli reset-password <email> <otp> <password> <confirm>\n"); return 1; }
    config_t cfg;
    config_load(&cfg);
    api_init();
    if (api_verify_otp(argv[0], argv[1]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (api_reset_password(argv[0], argv[2], argv[3]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"ok\":true}\n"); else fprintf(stderr, "Password reset. You can now login.\n");
    return 0;
}

static int cmd_feed(int argc, char **argv) {
    if (require_auth() != 0) return 1;
    int limit = 25, offset = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) limit = atoi(argv[++i]);
        else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atoi(argv[++i]);
    }
    api_posts_result_t result;
    if (api_fetch_followed_posts(offset, limit, &result) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_posts(&result);
    return 0;
}

static int cmd_post(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli post <post_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    api_post_t post;
    if (api_get_post_by_id(argv[0], &post) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_post(&post);

    api_comments_result_t comments;
    if (api_get_post_comments(argv[0], 0, 25, &comments) == 0 && comments.count > 0) {
        printf("\n%sComments (%d):%s\n", C_BOLD, comments.count, C_RESET);
        print_comments(&comments);
    }
    return 0;
}

static int cmd_create(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli create <text> [--media <file>]\n"); return 1; }
    if (require_auth() != 0) return 1;
    char *media_file = NULL;
    char *text_args[256];
    int text_count = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--media") == 0 && i + 1 < argc) { media_file = argv[++i]; }
        else if (text_count < 256) text_args[text_count++] = argv[i];
    }
    if (text_count == 0) { fprintf(stderr, "Usage: simple-social-cli create <text> [--media <file>]\n"); return 1; }
    char full_text[5000] = {0};
    size_t pos = 0;
    for (int i = 0; i < text_count; i++) {
        const char *src = text_args[i];
        size_t slen = strlen(src);
        size_t need = slen + (i > 0 ? 1 : 0);
        if (pos + need >= sizeof(full_text)) { need = sizeof(full_text) - 1 - pos; if (need == 0) break; if (i > 0 && pos + 1 < sizeof(full_text)) full_text[pos++] = ' '; size_t copy = need - (i > 0 ? 1 : 0); if (copy > slen) copy = slen; memcpy(full_text + pos, src, copy); pos += copy; full_text[pos] = '\0'; break; }
        if (i > 0) full_text[pos++] = ' ';
        memcpy(full_text + pos, src, slen);
        pos += slen;
        full_text[pos] = '\0';
    }
    char media_url[512] = {0};
    int media_id = 0;
    const char *media_url_ptr = NULL;
    if (media_file) {
        if (api_upload_media_with_id(media_file, media_url, sizeof(media_url), &media_id) != 0) {
            print_error(api_get_last_error());
            return 1;
        }
        media_url_ptr = media_url;
    }
    char post_id[64] = {0};
    if (api_create_post(full_text, media_url_ptr, post_id, sizeof(post_id)) != 0) {
        if (media_id) api_delete_media(media_id);
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) {
        if (media_url_ptr) printf("{\"postId\":\"%s\",\"mediaUrl\":\"%s\"}\n", post_id, media_url);
        else printf("{\"postId\":\"%s\"}\n", post_id);
    } else fprintf(stderr, "Created post %s\n", post_id);
    return 0;
}

static int cmd_delete_post(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli delete <post_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_delete_post(argv[0]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"deleted\":true,\"postId\":\"%s\"}\n", argv[0]); else fprintf(stderr, "Deleted post %s\n", argv[0]);
    return 0;
}

static int cmd_posts(int argc, char **argv) {
    if (require_auth() != 0) return 1;
    int limit = 25, offset = 0;
    int user_id = 0;
    int has_user = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) limit = atoi(argv[++i]);
        else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atoi(argv[++i]);
        else if (!has_user && argv[i][0] != '-') { user_id = atoi(argv[i]); has_user = 1; }
    }
    api_posts_result_t result;
    int rc;
    if (has_user) rc = api_get_user_posts(user_id, offset, limit, &result);
    else rc = api_get_my_posts(offset, limit, &result);
    if (rc != 0) { print_error(api_get_last_error()); return 1; }
    print_posts(&result);
    return 0;
}

static int cmd_likes(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli likes <post_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    char out[16384] = {0};
    if (api_get_post_likes(argv[0], out, sizeof(out)) != 0) { print_error(api_get_last_error()); return 1; }
    if (g_json_enabled) { printf("%s\n", out); return 0; }
    api_users_result_t res;
    res.count = 0;
    const char *arr_start, *arr_end;
    if (json_get_array(out, "likes", &arr_start, &arr_end) == 0 || json_get_array(out, "users", &arr_start, &arr_end) == 0) {
        int len = json_array_len(arr_start, arr_end);
        for (int i = 0; i < len && i < 256; i++) {
            const char *is, *ie;
            if (json_array_get_item(arr_start, arr_end, i, &is, &ie) != 0) break;
            int l = ie - is;
            char item[4096]; if (l >= (int)sizeof(item)) l = sizeof(item)-1; memcpy(item, is, l); item[l]='\0';
            api_user_t *u = &res.users[res.count];
            if (json_get_int(item, "userId", &u->id) != 0) json_get_int(item, "id", &u->id);
            json_get_string(item, "email", u->email, sizeof(u->email));
            if (u->email[0] == '\0') json_get_string(item, "userEmail", u->email, sizeof(u->email));
            json_get_string(item, "created_at", u->created_at, sizeof(u->created_at));
            res.count++;
        }
        print_users(&res);
        return 0;
    }
    printf("%s\n", out);
    return 0;
}

static int cmd_like(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli like <post_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_like_post(argv[0]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"liked\":true,\"postId\":\"%s\"}\n", argv[0]); else fprintf(stderr, "Liked post %s\n", argv[0]);
    return 0;
}

static int cmd_unlike(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli unlike <post_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_unlike_post(argv[0]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"liked\":false,\"postId\":\"%s\"}\n", argv[0]); else fprintf(stderr, "Unliked post %s\n", argv[0]);
    return 0;
}

static int cmd_comments(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli comments <post_id> [--offset N]\n"); return 1; }
    if (require_auth() != 0) return 1;
    const char *post_id = argv[0];
    int offset = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atoi(argv[++i]);
    }
    api_comments_result_t result;
    if (api_get_post_comments(post_id, offset, 25, &result) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_comments(&result);
    return 0;
}

static int cmd_comment(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: simple-social-cli comment <post_id> <text>\n"); return 1; }
    if (require_auth() != 0) return 1;
    const char *post_id = argv[0];
    char text[5000] = {0};
    size_t pos = 0;
    for (int i = 1; i < argc; i++) {
        const char *src = argv[i];
        size_t slen = strlen(src);
        size_t need = slen + (i > 1 ? 1 : 0);
        if (pos + need >= sizeof(text)) { need = sizeof(text) - 1 - pos; if (need == 0) break; if (i > 1 && pos + 1 < sizeof(text)) text[pos++] = ' '; size_t copy = need - (i > 1 ? 1 : 0); if (copy > slen) copy = slen; memcpy(text + pos, src, copy); pos += copy; text[pos] = '\0'; break; }
        if (i > 1) text[pos++] = ' ';
        memcpy(text + pos, src, slen);
        pos += slen;
        text[pos] = '\0';
    }
    int comment_id = 0;
    if (api_create_comment(post_id, text, &comment_id) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"commentId\":%d,\"postId\":\"%s\"}\n", comment_id, post_id); else fprintf(stderr, "Comment %d added to %s\n", comment_id, post_id);
    return 0;
}

static int cmd_delete_comment(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli delete-comment <comment_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_delete_comment(atoi(argv[0])) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"deleted\":true,\"commentId\":%s}\n", argv[0]); else fprintf(stderr, "Deleted comment %s\n", argv[0]);
    return 0;
}

static int cmd_users(int argc, char **argv) {
    (void)argc; (void)argv;
    if (require_auth() != 0) return 1;
    api_users_result_t result;
    if (api_get_users(&result) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_users(&result);
    return 0;
}

static int cmd_profile(int argc, char **argv) {
    if (require_auth() != 0) return 1;
    int user_id = state.user.user_id;
    if (argc >= 1) user_id = atoi(argv[0]);

    char email[256] = {0};
    char created[32] = {0};
    if (api_get_user_info(user_id, email, sizeof(email), created, sizeof(created)) != 0) {
        print_error(api_get_last_error());
        return 1;
    }

    api_users_result_t followers, following;
    api_get_my_followers(user_id, &followers);
    api_get_my_follows(user_id, &following);

    print_profile(user_id, email, created, followers.count, following.count);

    if (user_id != state.user.user_id) {
        int is_following = 0;
        api_is_following(user_id, &is_following);
        printf("%sFollowing:%s  %s\n", C_BOLD, C_RESET, is_following ? "Yes" : "No");
    }
    return 0;
}

static int cmd_follow(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli follow <user_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_follow_user(atoi(argv[0])) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"following\":true,\"userId\":%s}\n", argv[0]); else fprintf(stderr, "Followed user %s\n", argv[0]);
    return 0;
}

static int cmd_unfollow(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli unfollow <user_id>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_unfollow_user(atoi(argv[0])) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"following\":false,\"userId\":%s}\n", argv[0]); else fprintf(stderr, "Unfollowed user %s\n", argv[0]);
    return 0;
}

static int cmd_followers(int argc, char **argv) {
    if (require_auth() != 0) return 1;
    int user_id = state.user.user_id;
    if (argc >= 1) user_id = atoi(argv[0]);
    api_users_result_t result;
    if (api_get_my_followers(user_id, &result) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_users(&result);
    return 0;
}

static int cmd_following(int argc, char **argv) {
    if (require_auth() != 0) return 1;
    int user_id = state.user.user_id;
    if (argc >= 1) user_id = atoi(argv[0]);
    api_users_result_t result;
    if (api_get_my_follows(user_id, &result) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_users(&result);
    return 0;
}

static int cmd_notifications(int argc, char **argv) {
    if (require_auth() != 0) return 1;
    int offset = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atoi(argv[++i]);
    }
    api_notifications_result_t result;
    if (api_get_notifications(offset, &result) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    print_notifications(&result);
    return 0;
}

static int cmd_notify_count(int argc, char **argv) {
    (void)argc; (void)argv;
    if (require_auth() != 0) return 1;
    int count = 0;
    if (api_get_unseen_notification_count(&count) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) print_count_json(count, "count"); else print_count(count);
    return 0;
}

static int cmd_mark_seen(int argc, char **argv) {
    (void)argc; (void)argv;
    if (require_auth() != 0) return 1;
    if (api_mark_notifications_seen() != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    if (g_json_enabled) printf("{\"ok\":true}\n"); else fprintf(stderr, "Notifications marked as seen.\n");
    return 0;
}


static int cmd_delete_account(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "Usage: simple-social-cli delete-account <password>\n"); return 1; }
    if (require_auth() != 0) return 1;
    if (api_delete_account(argv[0]) != 0) {
        print_error(api_get_last_error());
        return 1;
    }
    ss_state_clear(&state);
    if (g_json_enabled) printf("{\"deleted\":true}\n"); else fprintf(stderr, "Account deleted.\n");
    return 0;
}

static void print_help(void) {
    fprintf(stderr, "Simple Social CLI\n\n");
    fprintf(stderr, "Usage: simple-social-cli <command> [args...]\n\n");
    fprintf(stderr, "AUTH\n");
    fprintf(stderr, "  login <email> <password>           Login\n");
    fprintf(stderr, "  logout                             Logout\n");
    fprintf(stderr, "  whoami                             Show current user\n");
    fprintf(stderr, "  register <email> <otp> <pw> <cfm>  Register\n");
    fprintf(stderr, "  send-otp <email>                   Send registration OTP\n");
    fprintf(stderr, "  reset-password <email> <otp> <pw> <cfm>  Reset password\n\n");
    fprintf(stderr, "POSTS\n");
    fprintf(stderr, "  feed [--limit N] [--offset N]      Show feed\n");
    fprintf(stderr, "  posts [user_id] [--limit N] [--offset N]  List user posts\n");
    fprintf(stderr, "  post <post_id>                     Show post + comments\n");
    fprintf(stderr, "  create <text> [--media <file>]    Create post\n");
    fprintf(stderr, "  delete <post_id>                   Delete post\n");
    fprintf(stderr, "  like <post_id>                     Like post\n");
    fprintf(stderr, "  unlike <post_id>                   Unlike post\n");
    fprintf(stderr, "  likes <post_id>                    List likes for post\n\n");
    fprintf(stderr, "COMMENTS\n");
    fprintf(stderr, "  comments <post_id> [--offset N]    Show comments\n");
    fprintf(stderr, "  comment <post_id> <text>           Add comment\n");
    fprintf(stderr, "  delete-comment <comment_id>        Delete comment\n\n");
    fprintf(stderr, "USERS\n");
    fprintf(stderr, "  users                              List all users\n");
    fprintf(stderr, "  profile [user_id]                  Show profile\n");
    fprintf(stderr, "  follow <user_id>                   Follow user\n");
    fprintf(stderr, "  unfollow <user_id>                 Unfollow user\n");
    fprintf(stderr, "  followers [user_id]                List followers\n");
    fprintf(stderr, "  following [user_id]                List following\n\n");
    fprintf(stderr, "NOTIFICATIONS\n");
    fprintf(stderr, "  notifications [--offset N]         Show notifications\n");
    fprintf(stderr, "  notify-count                       Unseen notification count\n");
    fprintf(stderr, "  mark-seen                          Mark notifications seen\n\n");

    fprintf(stderr, "ACCOUNT\n");
    fprintf(stderr, "  delete-account <password>          Delete your account\n\n");
    fprintf(stderr, "FLAGS\n");
    fprintf(stderr, "  --color          Colored output (auto if TTY)\n");
    fprintf(stderr, "  --json           Raw JSON output\n");
    fprintf(stderr, "  --help           Show this help\n");
}

typedef struct {
    const char *name;
    int min_args;
    int (*func)(int argc, char **argv);
} command_t;

static const command_t commands[] = {
    {"login",           2, cmd_login},
    {"logout",          0, cmd_logout},
    {"whoami",          0, cmd_whoami},
    {"register",        4, cmd_register},
    {"send-otp",        1, cmd_send_otp},
    {"reset-password",  4, cmd_reset_password},
    {"feed",            0, cmd_feed},
    {"posts",           0, cmd_posts},
    {"post",            1, cmd_post},
    {"create",          1, cmd_create},
    {"delete",          1, cmd_delete_post},
    {"like",            1, cmd_like},
    {"unlike",          1, cmd_unlike},
    {"likes",           1, cmd_likes},
    {"comments",        1, cmd_comments},
    {"comment",         2, cmd_comment},
    {"delete-comment",  1, cmd_delete_comment},
    {"users",           0, cmd_users},
    {"profile",         0, cmd_profile},
    {"follow",          1, cmd_follow},
    {"unfollow",        1, cmd_unfollow},
    {"followers",       0, cmd_followers},
    {"following",       0, cmd_following},
    {"notifications",   0, cmd_notifications},
    {"notify-count",    0, cmd_notify_count},
    {"mark-seen",       0, cmd_mark_seen},
    {"delete-account",  1, cmd_delete_account},
    {NULL, 0, NULL}
};

int main(int argc, char **argv) {
    ss_state_init(&state);

    if (argc < 2) {
        print_help();
        return 1;
    }

    int next_arg = 1;
    while (next_arg < argc && argv[next_arg][0] == '-') {
        if (strcmp(argv[next_arg], "--help") == 0) { print_help(); return 0; }
        else if (strcmp(argv[next_arg], "--color") == 0) g_color_enabled = 1;
        else if (strcmp(argv[next_arg], "--json") == 0) g_json_enabled = 1;
        else break;
        next_arg++;
    }
    if (next_arg >= argc) {
        print_help();
        return 1;
    }
    if (g_json_enabled) g_color_enabled = 0;
    else if (g_color_enabled == 0 && isatty(STDERR_FILENO)) g_color_enabled = 1;
    output_init();

    const char *cmd_name = argv[next_arg];
    next_arg++;

    if (strcmp(cmd_name, "login") != 0 && strcmp(cmd_name, "register") != 0 &&
        strcmp(cmd_name, "send-otp") != 0 && strcmp(cmd_name, "reset-password") != 0) {
        auto_login();
    }

    for (int i = 0; commands[i].name; i++) {
        if (strcmp(cmd_name, commands[i].name) == 0) {
            int cmd_argc = argc - next_arg;
            char **cmd_argv = argv + next_arg;
            if (cmd_argc < commands[i].min_args) {
                fprintf(stderr, "Error: %s requires at least %d argument(s)\n",
                        cmd_name, commands[i].min_args);
                return 1;
            }
            return commands[i].func(cmd_argc, cmd_argv);
        }
    }

    fprintf(stderr, "Unknown command: %s\n", cmd_name);
    fprintf(stderr, "Run 'simple-social-cli --help' for usage.\n");
    return 1;
}
