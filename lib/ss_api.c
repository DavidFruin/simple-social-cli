#include "ss_api.h"
#include "ss_json.h"
#include "ss_config.h"
#include "ss_utils.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char g_last_error[256] = {0};
static char g_jwt[1024] = {0};
static int g_user_id = 0;

void api_set_jwt(const char *jwt) {
    if (jwt) strncpy(g_jwt, jwt, sizeof(g_jwt) - 1);
    else g_jwt[0] = '\0';
}

void api_set_user_id(int user_id) {
    g_user_id = user_id;
}

struct write_result {
    char *data;
    size_t used;
    size_t size;
};

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    struct write_result *wr = (struct write_result *)userdata;
    size_t new_size = wr->used + size * nmemb;
    if (new_size >= wr->size) {
        wr->size = wr->size * 2;
        char *tmp = realloc(wr->data, wr->size);
        if (!tmp) return 0;
        wr->data = tmp;
    }
    memcpy(wr->data + wr->used, ptr, size * nmemb);
    wr->used += size * nmemb;
    wr->data[wr->used] = '\0';
    return size * nmemb;
}

int api_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

void api_cleanup(void) {
    curl_global_cleanup();
}

const char *api_get_last_error(void) {
    return g_last_error;
}

int api_call(const char *action, const char *params, char *response, int resp_size) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to init curl");
        return -1;
    }

    struct write_result wr;
    wr.data = malloc(resp_size);
    wr.used = 0;
    wr.size = resp_size;
    if (!wr.data) {
        curl_easy_cleanup(curl);
        return -1;
    }
    wr.data[0] = '\0';

    char post_fields[8192];
    if (params && params[0]) {
        snprintf(post_fields, sizeof(post_fields), "action=%s&%s", action, params);
    } else {
        snprintf(post_fields, sizeof(post_fields), "action=%s", action);
    }

    const char *base_url = config_get_base_url();

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    char auth_header[1152];
    if (g_jwt[0]) {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_jwt);
        headers = curl_slist_append(headers, auth_header);
    }

    curl_easy_setopt(curl, CURLOPT_URL, base_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        snprintf(g_last_error, sizeof(g_last_error), "HTTP error: %s", curl_easy_strerror(res));
        free(wr.data);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 400) {
        snprintf(g_last_error, sizeof(g_last_error), "HTTP %ld", http_code);
    }

    strncpy(response, wr.data, resp_size - 1);
    response[resp_size - 1] = '\0';
    free(wr.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return (http_code >= 200 && http_code < 400) ? 0 : -1;
}

static char *params_escape(const char *key, const char *val) {
    char *encoded_val = url_encode(val);
    if (!encoded_val) return NULL;
    int len = strlen(key) + strlen(encoded_val) + 2;
    char *result = malloc(len);
    if (result) snprintf(result, len, "%s=%s", key, encoded_val);
    free(encoded_val);
    return result;
}

static char *params_cat(char *params, const char *new_part) {
    if (!new_part) return params;
    if (!params) return strdup(new_part);
    int len = strlen(params) + strlen(new_part) + 2;
    char *result = malloc(len);
    if (result) {
        snprintf(result, len, "%s&%s", params, new_part);
        free(params);
    }
    return result;
}

static char *build_params(const char *key, const char *val) {
    return params_escape(key, val);
}

int api_login(const char *email, const char *password, char *jwt_out, int jwt_size, int *user_id_out) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    char *pw_part = params_escape("password", password);
    params = params_cat(params, pw_part);
    free(pw_part);

    int rc = api_call("login", params, resp, sizeof(resp));
    free(params);

    if (rc != 0) {
        char msg[256] = {0};
        json_get_string(resp, "message", msg, sizeof(msg));
        json_get_string(resp, "error", msg, sizeof(msg));
        snprintf(g_last_error, sizeof(g_last_error), "%s", msg[0] ? msg : "Login failed");
        return -1;
    }

    if (json_get_string(resp, "jwt", jwt_out, jwt_size) != 0) {
        snprintf(g_last_error, sizeof(g_last_error), "No JWT in response");
        return -1;
    }
    json_get_int(resp, "userId", user_id_out);
    return 0;
}

int api_logout(void) {
    char resp[API_MAX_RESPONSE];
    return api_call("logout", "", resp, sizeof(resp));
}

int api_get_my_info(int *id_out, char *email_out, int email_size, char *created_out, int created_size) {
    char resp[API_MAX_RESPONSE];
    int rc = api_call("getMyInfo", "", resp, sizeof(resp));
    if (rc != 0) return -1;
    json_get_int(resp, "userId", id_out);
    json_get_string(resp, "email", email_out, email_size);
    json_get_string(resp, "created_at", created_out, created_size);
    return 0;
}

int api_get_user_info(int user_id, char *email_out, int email_size, char *created_out, int created_size) {
    char resp[API_MAX_RESPONSE];
    char uid[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    char *params = build_params("userId", uid);
    int rc = api_call("getUserInfo", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;
    json_get_string(resp, "email", email_out, email_size);
    json_get_string(resp, "created_at", created_out, created_size);
    return 0;
}

int api_get_users(api_users_result_t *result) {
    char resp[API_MAX_RESPONSE];
    int rc = api_call("getUsers", "", resp, sizeof(resp));
    if (rc != 0) return -1;

    result->count = 0;
    const char *arr_start, *arr_end;
    if (json_get_array(resp, "users", &arr_start, &arr_end) != 0) return -1;

    int len = json_array_len(arr_start, arr_end);
    if (len > 256) len = 256;

    for (int i = 0; i < len; i++) {
        const char *item_start, *item_end;
        if (json_array_get_item(arr_start, arr_end, i, &item_start, &item_end) != 0) break;
        int item_len = item_end - item_start;
        char item[4096];
        if (item_len >= (int)sizeof(item)) item_len = sizeof(item) - 1;
        memcpy(item, item_start, item_len);
        item[item_len] = '\0';

        api_user_t *u = &result->users[result->count];
        json_get_int(item, "id", &u->id);
        json_get_string(item, "email", u->email, sizeof(u->email));
        json_get_string(item, "created_at", u->created_at, sizeof(u->created_at));
        result->count++;
    }
    return 0;
}

int api_get_user_emails(int *user_ids, int count, char *out, int out_size) {
    char ids_json[4096] = "[";
    for (int i = 0; i < count && i < 256; i++) {
        char num[16];
        snprintf(num, sizeof(num), "%d", user_ids[i]);
        if (i > 0) strcat(ids_json, ",");
        strcat(ids_json, num);
    }
    strcat(ids_json, "]");
    char *params = build_params("userIds", ids_json);
    int rc = api_call("getUserEmails", params, out, out_size);
    free(params);
    return rc;
}

int api_register_send_otp(const char *email) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    int rc = api_call("sendRegisterOTP", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_register_verify_otp(const char *email, const char *otp) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    char *otp_part = params_escape("otp", otp);
    params = params_cat(params, otp_part);
    free(otp_part);
    int rc = api_call("verifyRegisterOTP", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_register_finish(const char *email, const char *password, const char *confirm) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    char *part;
    part = params_escape("password", password);
    params = params_cat(params, part);
    free(part);
    part = params_escape("confirm", confirm);
    params = params_cat(params, part);
    free(part);
    int rc = api_call("finishRegister", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_send_otp(const char *email) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    int rc = api_call("sendOTP", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_verify_otp(const char *email, const char *otp) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    char *otp_part = params_escape("otp", otp);
    params = params_cat(params, otp_part);
    free(otp_part);
    int rc = api_call("verifyOTP", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_reset_password(const char *email, const char *password, const char *confirm) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("email", email);
    char *part;
    part = params_escape("password", password);
    params = params_cat(params, part);
    free(part);
    part = params_escape("confirm", confirm);
    params = params_cat(params, part);
    free(part);
    int rc = api_call("resetPassword", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_create_post(const char *text, const char *media_url, char *post_id_out, int post_id_size) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postText", text);
    if (media_url && media_url[0]) {
        char *mp = params_escape("mediaUrl", media_url);
        params = params_cat(params, mp);
        free(mp);
    }
    int rc = api_call("post", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    json_get_string(resp, "postId", post_id_out, post_id_size);
    return 0;
}

int api_delete_post(const char *post_id) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postId", post_id);
    int rc = api_call("deletePost", params, resp, sizeof(resp));
    free(params);
    return rc;
}

static int parse_posts(const char *resp, api_posts_result_t *result) {
    result->count = 0;
    result->has_more = 0;
    result->total_count = 0;
    json_get_bool(resp, "hasMore", &result->has_more);
    json_get_int(resp, "totalCount", &result->total_count);

    const char *arr_start, *arr_end;
    if (json_get_array(resp, "posts", &arr_start, &arr_end) != 0) return -1;

    int len = json_array_len(arr_start, arr_end);
    if (len > API_MAX_POSTS) len = API_MAX_POSTS;

    for (int i = 0; i < len; i++) {
        const char *item_start, *item_end;
        if (json_array_get_item(arr_start, arr_end, i, &item_start, &item_end) != 0) break;
        int item_len = item_end - item_start;
        char item[8192];
        if (item_len >= (int)sizeof(item)) item_len = sizeof(item) - 1;
        memcpy(item, item_start, item_len);
        item[item_len] = '\0';

        api_post_t *p = &result->posts[result->count];
        memset(p, 0, sizeof(*p));
        json_get_string(item, "id", p->id, sizeof(p->id));
        json_get_string(item, "text", p->text, sizeof(p->text));
        json_get_string(item, "timestamp", p->timestamp, sizeof(p->timestamp));
        json_get_string(item, "mediaUrl", p->media_url, sizeof(p->media_url));
        if (strcmp(p->media_url, "null") == 0) p->media_url[0] = '\0';
        json_get_int(item, "userID", &p->user_id);
        json_get_string(item, "userEmail", p->user_email, sizeof(p->user_email));

        const char *likes_start, *likes_end;
        if (json_get_array(item, "likes", &likes_start, &likes_end) == 0) {
            int num_likes = json_array_len(likes_start, likes_end);
            p->like_count = num_likes;
            p->is_liked = 0;
            if (g_user_id > 0) {
                for (int j = 0; j < num_likes; j++) {
                    const char *like_item_start, *like_item_end;
                    if (json_array_get_item(likes_start, likes_end, j, &like_item_start, &like_item_end) == 0) {
                        char like_item[256];
                        int like_len = like_item_end - like_item_start;
                        if (like_len >= (int)sizeof(like_item)) like_len = sizeof(like_item) - 1;
                        memcpy(like_item, like_item_start, like_len);
                        like_item[like_len] = '\0';
                        int like_user_id = 0;
                        json_get_int(like_item, "userId", &like_user_id);
                        if (like_user_id == g_user_id) {
                            p->is_liked = 1;
                            break;
                        }
                    }
                }
            }
        }
        result->count++;
    }
    return 0;
}

int api_get_my_posts(int offset, int limit, api_posts_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char off_str[16], lim_str[16];
    snprintf(off_str, sizeof(off_str), "%d", offset);
    snprintf(lim_str, sizeof(lim_str), "%d", limit);
    char *params = build_params("offset", off_str);
    char *lp = params_escape("limit", lim_str);
    params = params_cat(params, lp);
    free(lp);
    int rc = api_call("getMyPosts", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;
    return parse_posts(resp, result);
}

int api_get_user_posts(int user_id, int offset, int limit, api_posts_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char uid[16], off_str[16], lim_str[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    snprintf(off_str, sizeof(off_str), "%d", offset);
    snprintf(lim_str, sizeof(lim_str), "%d", limit);
    char *params = build_params("userId", uid);
    char *p;
    p = params_escape("offset", off_str);
    params = params_cat(params, p);
    free(p);
    p = params_escape("limit", lim_str);
    params = params_cat(params, p);
    free(p);
    int rc = api_call("getUserPosts", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;
    return parse_posts(resp, result);
}

int api_fetch_followed_posts(int offset, int limit, api_posts_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char off_str[16], lim_str[16];
    snprintf(off_str, sizeof(off_str), "%d", offset);
    snprintf(lim_str, sizeof(lim_str), "%d", limit);
    char *params = build_params("offset", off_str);
    char *lp = params_escape("limit", lim_str);
    params = params_cat(params, lp);
    free(lp);
    int rc = api_call("fetchFollowedPosts", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;
    return parse_posts(resp, result);
}

int api_like_post(const char *post_id) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postId", post_id);
    int rc = api_call("likePost", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    return 0;
}

int api_unlike_post(const char *post_id) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postId", post_id);
    int rc = api_call("unlikePost", params, resp, sizeof(resp));
    free(params);
    return rc;
}

int api_get_post_likes(const char *post_id, char *out, int out_size) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postId", post_id);
    int rc = api_call("getPostLikes", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;
    strncpy(out, resp, out_size - 1);
    return 0;
}

int api_follow_user(int user_id) {
    char resp[API_MAX_RESPONSE];
    char uid[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    char *params = build_params("userId", uid);
    int rc = api_call("followUser", params, resp, sizeof(resp));
    free(params);
    return rc;
}

int api_unfollow_user(int user_id) {
    char resp[API_MAX_RESPONSE];
    char uid[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    char *params = build_params("userId", uid);
    int rc = api_call("unfollowUser", params, resp, sizeof(resp));
    free(params);
    return rc;
}

int api_is_following(int user_id, int *out) {
    char resp[API_MAX_RESPONSE];
    char uid[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    char *params = build_params("userId", uid);
    int rc = api_call("isFollowing", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;
    json_get_bool(resp, "following", out);
    return 0;
}

int api_get_my_follows(int user_id, api_users_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char uid[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    char *params = build_params("userId", uid);
    int rc = api_call("getMyFollows", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;

    result->count = 0;
    const char *arr_start, *arr_end;
    if (json_get_array(resp, "follows", &arr_start, &arr_end) != 0) return -1;
    int len = json_array_len(arr_start, arr_end);
    if (len > 256) len = 256;
    for (int i = 0; i < len; i++) {
        const char *item_start, *item_end;
        if (json_array_get_item(arr_start, arr_end, i, &item_start, &item_end) != 0) break;
        int item_len = item_end - item_start;
        char item[4096];
        if (item_len >= (int)sizeof(item)) item_len = sizeof(item) - 1;
        memcpy(item, item_start, item_len);
        item[item_len] = '\0';
        api_user_t *u = &result->users[result->count];
        json_get_int(item, "id", &u->id);
        json_get_string(item, "email", u->email, sizeof(u->email));
        if (json_get_string(item, "timestamp", u->created_at, sizeof(u->created_at)) != 0)
            json_get_string(item, "created_at", u->created_at, sizeof(u->created_at));
        result->count++;
    }
    return 0;
}

int api_get_my_followers(int user_id, api_users_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char uid[16];
    snprintf(uid, sizeof(uid), "%d", user_id);
    char *params = build_params("userId", uid);
    int rc = api_call("getMyFollowers", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;

    result->count = 0;
    const char *arr_start, *arr_end;
    if (json_get_array(resp, "followers", &arr_start, &arr_end) != 0) return -1;
    int len = json_array_len(arr_start, arr_end);
    if (len > 256) len = 256;
    for (int i = 0; i < len; i++) {
        const char *item_start, *item_end;
        if (json_array_get_item(arr_start, arr_end, i, &item_start, &item_end) != 0) break;
        int item_len = item_end - item_start;
        char item[4096];
        if (item_len >= (int)sizeof(item)) item_len = sizeof(item) - 1;
        memcpy(item, item_start, item_len);
        item[item_len] = '\0';
        api_user_t *u = &result->users[result->count];
        json_get_int(item, "id", &u->id);
        json_get_string(item, "email", u->email, sizeof(u->email));
        if (json_get_string(item, "timestamp", u->created_at, sizeof(u->created_at)) != 0)
            json_get_string(item, "created_at", u->created_at, sizeof(u->created_at));
        result->count++;
    }
    return 0;
}

int api_create_comment(const char *post_id, const char *text, int *comment_id_out) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postId", post_id);
    char *tp = params_escape("text", text);
    params = params_cat(params, tp);
    free(tp);
    int rc = api_call("createComment", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) {
        json_get_string(resp, "message", g_last_error, sizeof(g_last_error));
        return -1;
    }
    json_get_int(resp, "commentId", comment_id_out);
    return 0;
}

int api_get_post_comments(const char *post_id, int offset, int limit, api_comments_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char off_str[16], lim_str[16];
    snprintf(off_str, sizeof(off_str), "%d", offset);
    snprintf(lim_str, sizeof(lim_str), "%d", limit);
    char *params = build_params("postId", post_id);
    char *p;
    p = params_escape("offset", off_str);
    params = params_cat(params, p);
    free(p);
    p = params_escape("limit", lim_str);
    params = params_cat(params, p);
    free(p);
    int rc = api_call("getPostComments", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;

    result->count = 0;
    result->has_more = 0;
    result->total_count = 0;
    json_get_bool(resp, "hasMore", &result->has_more);
    json_get_int(resp, "totalCount", &result->total_count);

    const char *arr_start, *arr_end;
    if (json_get_array(resp, "comments", &arr_start, &arr_end) != 0) return -1;
    int len = json_array_len(arr_start, arr_end);
    if (len > 256) len = 256;
    for (int i = 0; i < len; i++) {
        const char *item_start, *item_end;
        if (json_array_get_item(arr_start, arr_end, i, &item_start, &item_end) != 0) break;
        int item_len = item_end - item_start;
        char item[8192];
        if (item_len >= (int)sizeof(item)) item_len = sizeof(item) - 1;
        memcpy(item, item_start, item_len);
        item[item_len] = '\0';
        api_comment_t *c = &result->comments[result->count];
        memset(c, 0, sizeof(*c));
        json_get_int(item, "id", &c->id);
        json_get_int(item, "user_id", &c->user_id);
        json_get_string(item, "text", c->text, sizeof(c->text));
        json_get_string(item, "created_at", c->created_at, sizeof(c->created_at));
        json_get_string(item, "user_email", c->user_email, sizeof(c->user_email));
        result->count++;
    }
    return 0;
}

int api_delete_comment(int comment_id) {
    char resp[API_MAX_RESPONSE];
    char cid[16];
    snprintf(cid, sizeof(cid), "%d", comment_id);
    char *params = build_params("commentId", cid);
    int rc = api_call("deleteComment", params, resp, sizeof(resp));
    free(params);
    return rc;
}

int api_get_post_comment_counts(char post_ids[][64], int count, char *out, int out_size) {
    char ids_json[16384] = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(ids_json, ",");
        strcat(ids_json, "\"");
        strcat(ids_json, post_ids[i]);
        strcat(ids_json, "\"");
    }
    strcat(ids_json, "]");
    char *params = build_params("postIds", ids_json);
    int rc = api_call("getPostCommentCounts", params, out, out_size);
    free(params);
    return rc;
}

int api_get_notifications(int offset, api_notifications_result_t *result) {
    char resp[API_MAX_RESPONSE];
    char off_str[16];
    snprintf(off_str, sizeof(off_str), "%d", offset);
    char *params = build_params("offset", off_str);
    int rc = api_call("getNotifications", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;

    result->count = 0;
    const char *arr_start, *arr_end;
    if (json_get_array(resp, "notifications", &arr_start, &arr_end) != 0) return -1;
    int len = json_array_len(arr_start, arr_end);
    if (len > 256) len = 256;
    for (int i = 0; i < len; i++) {
        const char *item_start, *item_end;
        if (json_array_get_item(arr_start, arr_end, i, &item_start, &item_end) != 0) break;
        int item_len = item_end - item_start;
        char item[4096];
        if (item_len >= (int)sizeof(item)) item_len = sizeof(item) - 1;
        memcpy(item, item_start, item_len);
        item[item_len] = '\0';
        api_notification_t *n = &result->notifications[result->count];
        memset(n, 0, sizeof(*n));
        json_get_int(item, "id", &n->id);
        json_get_int(item, "recipient_id", &n->recipient_id);
        json_get_int(item, "actor_id", &n->actor_id);
        json_get_string(item, "actor_email", n->actor_email, sizeof(n->actor_email));
        json_get_string(item, "type", n->type, sizeof(n->type));
        json_get_string(item, "post_id", n->post_id, sizeof(n->post_id));
        json_get_string(item, "created_at", n->created_at, sizeof(n->created_at));
        result->count++;
    }
    return 0;
}

int api_get_unseen_notification_count(int *count_out) {
    char resp[API_MAX_RESPONSE];
    int rc = api_call("getUnseenNotificationCount", "", resp, sizeof(resp));
    if (rc != 0) return -1;
    json_get_int(resp, "count", count_out);
    return 0;
}

int api_mark_notifications_seen(void) {
    char resp[API_MAX_RESPONSE];
    return api_call("markNotificationsSeen", "", resp, sizeof(resp));
}

int api_get_post_by_id(const char *post_id, api_post_t *post_out) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("postId", post_id);
    int rc = api_call("getPostById", params, resp, sizeof(resp));
    free(params);
    if (rc != 0) return -1;

    const char *post_start, *post_end;
    if (json_get_nested(resp, "post", &post_start, &post_end) != 0) return -1;
    int post_len = post_end - post_start;
    char post_json[8192];
    if (post_len >= (int)sizeof(post_json)) post_len = sizeof(post_json) - 1;
    memcpy(post_json, post_start, post_len);
    post_json[post_len] = '\0';

    memset(post_out, 0, sizeof(*post_out));
    json_get_string(post_json, "id", post_out->id, sizeof(post_out->id));
    json_get_string(post_json, "text", post_out->text, sizeof(post_out->text));
    json_get_string(post_json, "timestamp", post_out->timestamp, sizeof(post_out->timestamp));
    json_get_string(post_json, "mediaUrl", post_out->media_url, sizeof(post_out->media_url));
    if (strcmp(post_out->media_url, "null") == 0) post_out->media_url[0] = '\0';
    json_get_int(post_json, "userID", &post_out->user_id);
    json_get_string(post_json, "userEmail", post_out->user_email, sizeof(post_out->user_email));

    const char *likes_start, *likes_end;
    if (json_get_array(post_json, "likes", &likes_start, &likes_end) == 0) {
        int num_likes = json_array_len(likes_start, likes_end);
        post_out->like_count = num_likes;
        post_out->is_liked = 0;
        if (g_user_id > 0) {
            for (int j = 0; j < num_likes; j++) {
                const char *like_item_start, *like_item_end;
                if (json_array_get_item(likes_start, likes_end, j, &like_item_start, &like_item_end) == 0) {
                    char like_item[256];
                    int like_len = like_item_end - like_item_start;
                    if (like_len >= (int)sizeof(like_item)) like_len = sizeof(like_item) - 1;
                    memcpy(like_item, like_item_start, like_len);
                    like_item[like_len] = '\0';
                    int like_user_id = 0;
                    json_get_int(like_item, "userId", &like_user_id);
                    if (like_user_id == g_user_id) {
                        post_out->is_liked = 1;
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

int api_delete_account(const char *password) {
    char resp[API_MAX_RESPONSE];
    char *params = build_params("password", password);
    int rc = api_call("deleteAccount", params, resp, sizeof(resp));
    free(params);
    return rc;
}

int api_upload_media(const char *filepath, char *url_out, int url_size) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_filedata(part, filepath);
    curl_mime_name(part, "file");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, "uploadMedia", CURL_ZERO_TERMINATED);
    curl_mime_name(part, "action");

    struct write_result wr;
    wr.data = malloc(API_MAX_RESPONSE);
    wr.used = 0;
    wr.size = API_MAX_RESPONSE;
    wr.data[0] = '\0';

    const char *base_url = config_get_base_url();
    char media_url[512];
    snprintf(media_url, sizeof(media_url), "%s", base_url);
    char *slash = strrchr(media_url, '/');
    if (slash) *slash = '\0';
    strcat(media_url, "/media.php");

    struct curl_slist *headers = NULL;
    char auth_header[1152];
    if (g_jwt[0]) {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_jwt);
        headers = curl_slist_append(headers, auth_header);
    }

    curl_easy_setopt(curl, CURLOPT_URL, media_url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    curl_mime_free(mime);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        free(wr.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    if (json_get_string(wr.data, "mediaUrl", url_out, url_size) != 0) {
        free(wr.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    free(wr.data);
    curl_easy_cleanup(curl);
    return 0;
}

int api_delete_media(int media_id) {
    char resp[API_MAX_RESPONSE];
    char mid[16];
    snprintf(mid, sizeof(mid), "%d", media_id);
    const char *base_url = config_get_base_url();
    char media_url[512];
    snprintf(media_url, sizeof(media_url), "%s", base_url);
    char *slash = strrchr(media_url, '/');
    if (slash) *slash = '\0';
    strcat(media_url, "/media.php");
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    struct write_result wr;
    wr.data = malloc(API_MAX_RESPONSE);
    wr.used = 0;
    wr.size = API_MAX_RESPONSE;
    wr.data[0] = '\0';
    char post_fields[256];
    snprintf(post_fields, sizeof(post_fields), "action=deleteMedia&mediaId=%s", mid);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    char auth_header[1152];
    if (g_jwt[0]) {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_jwt);
        headers = curl_slist_append(headers, auth_header);
    }
    curl_easy_setopt(curl, CURLOPT_URL, media_url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    int rc = 0;
    if (res != CURLE_OK) rc = -1;
    else {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        if (code < 200 || code >= 400) rc = -1;
    }
    free(wr.data);
    return rc;
}

int api_upload_media_with_id(const char *filepath, char *url_out, int url_size, int *id_out) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_filedata(part, filepath);
    curl_mime_name(part, "file");
    part = curl_mime_addpart(mime);
    curl_mime_data(part, "uploadMedia", CURL_ZERO_TERMINATED);
    curl_mime_name(part, "action");
    struct write_result wr;
    wr.data = malloc(API_MAX_RESPONSE);
    wr.used = 0;
    wr.size = API_MAX_RESPONSE;
    wr.data[0] = '\0';
    const char *base_url = config_get_base_url();
    char media_url[512];
    snprintf(media_url, sizeof(media_url), "%s", base_url);
    char *slash = strrchr(media_url, '/');
    if (slash) *slash = '\0';
    strcat(media_url, "/media.php");
    struct curl_slist *headers = NULL;
    char auth_header2[1152];
    if (g_jwt[0]) {
        snprintf(auth_header2, sizeof(auth_header2), "Authorization: Bearer %s", g_jwt);
        headers = curl_slist_append(headers, auth_header2);
    }
    curl_easy_setopt(curl, CURLOPT_URL, media_url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    CURLcode res2 = curl_easy_perform(curl);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    if (res2 != CURLE_OK) { free(wr.data); curl_easy_cleanup(curl); return -1; }
    int rc2 = 0;
    if (json_get_string(wr.data, "mediaUrl", url_out, url_size) != 0) rc2 = -1;
    if (rc2 == 0 && id_out) {
        int mid2 = 0;
        if (json_get_int(wr.data, "mediaId", &mid2) == 0) *id_out = mid2;
        else *id_out = 0;
    }
    free(wr.data);
    curl_easy_cleanup(curl);
    return rc2;
}
