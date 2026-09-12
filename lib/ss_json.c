#include "ss_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *find_key(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    while (p) {
        const char *colon = strchr(p + strlen(search), ':');
        if (colon) {
            const char *ws = colon + 1;
            while (*ws && isspace((unsigned char)*ws)) ws++;
            return ws;
        }
        p = strstr(p + strlen(search), search);
    }
    return NULL;
}

static const char *skip_string(const char *s) {
    if (*s != '"') return s;
    s++;
    while (*s) {
        if (*s == '\\') { s += 2; continue; }
        if (*s == '"') return s + 1;
        s++;
    }
    return s;
}

static const char *skip_value(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '"') return skip_string(s);
    if (*s == '{' || *s == '[') {
        char open = *s;
        char close = (open == '{') ? '}' : ']';
        int depth = 1;
        s++;
        while (*s && depth > 0) {
            if (*s == open) depth++;
            else if (*s == close) depth--;
            else if (*s == '"') { s = skip_string(s); continue; }
            s++;
        }
        return s;
    }
    while (*s && *s != ',' && *s != '}' && *s != ']') s++;
    return s;
}

int json_get_string(const char *json, const char *key, char *out, int out_size) {
    const char *val = find_key(json, key);
    if (!val) return -1;
    if (*val != '"') return -1;
    val++;
    const char *end = val;
    while (*end && *end != '"') {
        if (*end == '\\') end++;
        end++;
    }
    int len = end - val;
    if (len >= out_size) len = out_size - 1;
    strncpy(out, val, len);
    out[len] = '\0';
    return 0;
}

int json_get_int(const char *json, const char *key, int *out) {
    const char *val = find_key(json, key);
    if (!val) return -1;
    *out = atoi(val);
    return 0;
}

int json_get_bool(const char *json, const char *key, int *out) {
    const char *val = find_key(json, key);
    if (!val) return -1;
    if (strncmp(val, "true", 4) == 0) { *out = 1; return 0; }
    if (strncmp(val, "false", 5) == 0) { *out = 0; return 0; }
    *out = atoi(val);
    return 0;
}

int json_get_nested(const char *json, const char *key, const char **start, const char **end) {
    const char *val = find_key(json, key);
    if (!val) return -1;
    while (*val && isspace((unsigned char)*val)) val++;
    if (*val != '{' && *val != '[') return -1;
    char open = *val;
    char close = (open == '{') ? '}' : ']';
    const char *p = val + 1;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == open) depth++;
        else if (*p == close) depth--;
        else if (*p == '"') { p = skip_string(p); continue; }
        p++;
    }
    *start = val;
    *end = p;
    return 0;
}

int json_get_array(const char *json, const char *key, const char **start, const char **end) {
    return json_get_nested(json, key, start, end);
}

int json_array_len(const char *arr_start, const char *arr_end) {
    if (!arr_start || !arr_end || *arr_start != '[') return 0;
    int count = 0;
    int depth = 0;
    const char *p = arr_start + 1;
    while (p < arr_end) {
        if (*p == '[' || *p == '{') depth++;
        else if (*p == ']' || *p == '}') depth--;
        else if (*p == ',' && depth == 0) count++;
        else if (*p == '"') { p = skip_string(p); continue; }
        p++;
    }
    if (arr_end - arr_start > 2) count++;
    return count;
}

int json_array_get_item(const char *arr_start, const char *arr_end, int index,
                        const char **item_start, const char **item_end) {
    if (!arr_start || !arr_end || *arr_start != '[') return -1;
    int count = 0;
    int depth = 0;
    const char *p = arr_start + 1;
    const char *item = p;
    while (p < arr_end) {
        if (*p == '[' || *p == '{') depth++;
        else if (*p == ']' || *p == '}') {
            depth--;
            if (depth < 0) {
                if (count == index) {
                    *item_start = item;
                    *item_end = p;
                    return 0;
                }
                return -1;
            }
        } else if (*p == ',' && depth == 0) {
            if (count == index) {
                *item_start = item;
                *item_end = p;
                return 0;
            }
            count++;
            item = p + 1;
            while (*item && isspace((unsigned char)*item)) item++;
        } else if (*p == '"') { p = skip_string(p); continue; }
        p++;
    }
    if (count == index && item < arr_end) {
        *item_start = item;
        *item_end = arr_end - 1;
        return 0;
    }
    return -1;
}
