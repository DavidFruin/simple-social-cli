#include "output.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_color_enabled = 0;
int g_json_enabled = 0;

const char *C_RESET = "";
const char *C_BOLD = "";
const char *C_CYAN = "";
const char *C_GREEN = "";
const char *C_YELLOW = "";
const char *C_RED = "";
const char *C_DIM = "";

void output_init(void) {
    if (g_color_enabled) {
        C_RESET = "\033[0m";
        C_BOLD = "\033[1m";
        C_CYAN = "\033[36m";
        C_GREEN = "\033[32m";
        C_YELLOW = "\033[33m";
        C_RED = "\033[31m";
        C_DIM = "\033[2m";
    }
}

static void json_escape(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') fputs("\\\"", f);
        else if (c == '\\') fputs("\\\\", f);
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\r') fputs("\\r", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c < 0x20) fprintf(f, "\\u%04x", c);
        else fputc(c, f);
    }
    fputc('"', f);
}

static int is_null_media(const char *s) {
    return s[0] == '\0' || strcmp(s, "null") == 0;
}

void print_posts(const api_posts_result_t *result) {
    if (g_json_enabled) {
        printf("{\"posts\":[");
        for (int i = 0; i < result->count; i++) {
            const api_post_t *p = &result->posts[i];
            if (i) printf(",");
            printf("{\"id\":");
            json_escape(stdout, p->id);
            printf(",\"text\":");
            json_escape(stdout, p->text);
            printf(",\"timestamp\":");
            json_escape(stdout, p->timestamp);
            printf(",\"userID\":%d,\"userEmail\":", p->user_id);
            json_escape(stdout, p->user_email);
            printf(",\"likeCount\":%d,\"isLiked\":%s", p->like_count, p->is_liked ? "true" : "false");
            if (!is_null_media(p->media_url)) { printf(",\"mediaUrl\":"); json_escape(stdout, p->media_url); } else printf(",\"mediaUrl\":null");
            printf(",\"likes\":%d}", p->like_count);
        }
        printf("],\"hasMore\":%s,\"totalCount\":%d}\n", result->has_more ? "true" : "false", result->total_count);
        return;
    }
    if (result->count == 0) {
        fprintf(stderr, "No posts found.\n");
        return;
    }
    printf("%s%-10s  %-40s  %s%-6s  %s%s\n", C_BOLD, "ID", "Author", C_RESET, "Likes", C_BOLD, "Text");
    printf("%s", C_DIM);
    for (int i = 0; i < 70; i++) putchar('-');
    printf("%s\n", C_RESET);
    for (int i = 0; i < result->count; i++) {
        const api_post_t *p = &result->posts[i];
        char text[41] = {0};
        strncpy(text, p->text, 40);
        if (strlen(p->text) > 40) strcpy(text + 37, "...");
        char id_short[11] = {0};
        strncpy(id_short, p->id, 10);
        if (strlen(p->id) > 10) strcpy(id_short + 7, "...");
        if (p->is_liked)
            printf("%-10s  %-40s  %s%*d%s  %s\n", id_short, p->user_email, C_GREEN, 6, p->like_count, C_RESET, text);
        else
            printf("%-10s  %-40s  %*d  %s\n", id_short, p->user_email, 6, p->like_count, text);
    }
}

void print_post(const api_post_t *post) {
    if (g_json_enabled) {
        printf("{\"post\":{\"id\":");
        json_escape(stdout, post->id);
        printf(",\"text\":");
        json_escape(stdout, post->text);
        printf(",\"timestamp\":");
        json_escape(stdout, post->timestamp);
        printf(",\"userID\":%d,\"userEmail\":", post->user_id);
        json_escape(stdout, post->user_email);
        printf(",\"likeCount\":%d,\"isLiked\":%s", post->like_count, post->is_liked ? "true" : "false");
        if (!is_null_media(post->media_url)) { printf(",\"mediaUrl\":"); json_escape(stdout, post->media_url); } else printf(",\"mediaUrl\":null");
        printf("}}\n");
        return;
    }
    printf("%sPost:%s %s\n", C_BOLD, C_RESET, post->id);
    printf("%sAuthor:%s %s (ID: %d)\n", C_BOLD, C_RESET, post->user_email, post->user_id);
    printf("%sDate:%s   %s\n", C_BOLD, C_RESET, post->timestamp);
    printf("%sLikes:%s  %d", C_BOLD, C_RESET, post->like_count);
    if (post->is_liked)
        printf(" %s(you liked this)%s", C_GREEN, C_RESET);
    printf("\n");
    if (!is_null_media(post->media_url))
        printf("%sMedia:%s  %s\n", C_BOLD, C_RESET, post->media_url);
    printf("\n%s%s%s\n", C_BOLD, post->text, C_RESET);
}

void print_users(const api_users_result_t *result) {
    if (g_json_enabled) {
        printf("{\"users\":[");
        for (int i = 0; i < result->count; i++) {
            const api_user_t *u = &result->users[i];
            if (i) printf(",");
            printf("{\"id\":%d,\"email\":", u->id);
            json_escape(stdout, u->email);
            printf(",\"created_at\":");
            json_escape(stdout, u->created_at);
            printf("}");
        }
        printf("]}\n");
        return;
    }
    if (result->count == 0) {
        fprintf(stderr, "No users found.\n");
        return;
    }
    printf("%s%-6s  %-40s  %s%s\n", C_BOLD, "ID", "Email", "Joined", C_RESET);
    printf("%s", C_DIM);
    for (int i = 0; i < 60; i++) putchar('-');
    printf("%s\n", C_RESET);
    for (int i = 0; i < result->count; i++) {
        const api_user_t *u = &result->users[i];
        printf("%-6d  %-40s  %s\n", u->id, u->email, u->created_at);
    }
}

void print_user_info(int id, const char *email, const char *created_at) {
    if (g_json_enabled) {
        printf("{\"id\":%d,\"email\":", id);
        json_escape(stdout, email);
        printf(",\"created_at\":");
        json_escape(stdout, created_at);
        printf("}\n");
        return;
    }
    printf("%sID:%s     %d\n", C_BOLD, C_RESET, id);
    printf("%sEmail:%s  %s\n", C_BOLD, C_RESET, email);
    printf("%sJoined:%s %s\n", C_BOLD, C_RESET, created_at);
}

void print_comments(const api_comments_result_t *result) {
    if (g_json_enabled) {
        printf("{\"comments\":[");
        for (int i = 0; i < result->count; i++) {
            const api_comment_t *c = &result->comments[i];
            if (i) printf(",");
            printf("{\"id\":%d,\"user_id\":%d,\"user_email\":", c->id, c->user_id);
            json_escape(stdout, c->user_email);
            printf(",\"text\":");
            json_escape(stdout, c->text);
            printf(",\"created_at\":");
            json_escape(stdout, c->created_at);
            printf("}");
        }
        printf("],\"hasMore\":%s,\"totalCount\":%d}\n", result->has_more ? "true" : "false", result->total_count);
        return;
    }
    if (result->count == 0) {
        printf("No comments.\n");
        return;
    }
    for (int i = 0; i < result->count; i++) {
        const api_comment_t *c = &result->comments[i];
        printf("  %s[%d]%s %s%s (%s)%s: %s\n",
               C_DIM, c->id, C_RESET,
               C_CYAN, c->user_email, c->created_at, C_RESET,
               c->text);
    }
}

void print_notifications(const api_notifications_result_t *result) {
    if (g_json_enabled) {
        printf("{\"notifications\":[");
        for (int i = 0; i < result->count; i++) {
            const api_notification_t *n = &result->notifications[i];
            if (i) printf(",");
            printf("{\"id\":%d,\"recipient_id\":%d,\"actor_id\":%d,\"actor_email\":", n->id, n->recipient_id, n->actor_id);
            json_escape(stdout, n->actor_email);
            printf(",\"type\":");
            json_escape(stdout, n->type);
            printf(",\"post_id\":");
            if (n->post_id[0]) json_escape(stdout, n->post_id); else printf("null");
            printf(",\"created_at\":");
            json_escape(stdout, n->created_at);
            printf("}");
        }
        printf("]}\n");
        return;
    }
    if (result->count == 0) {
        printf("No notifications.\n");
        return;
    }
    printf("%s%-12s  %-40s  %-20s  %s%s\n", C_BOLD, "Type", "Actor", "Post", "Date", C_RESET);
    printf("%s", C_DIM);
    for (int i = 0; i < 80; i++) putchar('-');
    printf("%s\n", C_RESET);
    for (int i = 0; i < result->count; i++) {
        const api_notification_t *n = &result->notifications[i];
        const char *type_color = C_RESET;
        if (strcmp(n->type, "like") == 0 || strcmp(n->type, "unlike") == 0) type_color = C_YELLOW;
        else if (strcmp(n->type, "follow") == 0 || strcmp(n->type, "unfollow") == 0) type_color = C_CYAN;
        else if (strcmp(n->type, "comment") == 0) type_color = C_GREEN;
        printf("%s%-12s%s  %-40s  %-20s  %s\n",
               type_color, n->type, C_RESET,
               n->actor_email,
               n->post_id[0] ? n->post_id : "-",
               n->created_at);
    }
}

void print_profile(int user_id, const char *email, const char *created_at,
                   int follower_count, int following_count) {
    if (g_json_enabled) {
        printf("{\"id\":%d,\"email\":", user_id);
        json_escape(stdout, email);
        printf(",\"created_at\":");
        json_escape(stdout, created_at);
        printf(",\"followers\":%d,\"following\":%d}\n", follower_count, following_count);
        return;
    }
    printf("%sID:%s         %d\n", C_BOLD, C_RESET, user_id);
    printf("%sEmail:%s      %s\n", C_BOLD, C_RESET, email);
    printf("%sJoined:%s     %s\n", C_BOLD, C_RESET, created_at);
    printf("%sFollowers:%s  %d\n", C_BOLD, C_RESET, follower_count);
    printf("%sFollowing:%s  %d\n", C_BOLD, C_RESET, following_count);
}

void print_success(const char *msg) {
    if (g_json_enabled) {
        printf("{\"ok\":true,\"message\":");
        json_escape(stdout, msg);
        printf("}\n");
        return;
    }
    fprintf(stderr, "%s%s%s\n", C_GREEN, msg, C_RESET);
}

void print_error(const char *msg) {
    if (g_json_enabled) {
        printf("{\"ok\":false,\"error\":");
        json_escape(stdout, msg);
        printf("}\n");
    }
    fprintf(stderr, "%sError: %s%s\n", C_RED, msg, C_RESET);
}

void print_json_error(const char *msg) {
    printf("{\"ok\":false,\"error\":");
    json_escape(stdout, msg);
    printf("}\n");
    fprintf(stderr, "%sError: %s%s\n", C_RED, msg, C_RESET);
}

void print_count(int count) {
    printf("%d\n", count);
}

void print_count_json(int count, const char *key) {
    printf("{\"%s\":%d}\n", key, count);
}
