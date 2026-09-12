#include "ss_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *str_trim(char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

char *str_dup(const char *s) {
    if (!s) return NULL;
    char *r = malloc(strlen(s) + 1);
    if (r) strcpy(r, s);
    return r;
}

char *str_lower(const char *s) {
    if (!s) return NULL;
    char *r = malloc(strlen(s) + 1);
    if (!r) return NULL;
    for (int i = 0; s[i]; i++) r[i] = tolower((unsigned char)s[i]);
    r[strlen(s)] = '\0';
    return r;
}

int str_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

char *str_replace(const char *orig, const char *rep, const char *with) {
    if (!orig || !rep || !with) return str_dup(orig);
    int rep_len = strlen(rep);
    int with_len = strlen(with);
    int count = 0;
    const char *tmp = orig;
    while ((tmp = strstr(tmp, rep))) { count++; tmp += rep_len; }
    if (count == 0) return str_dup(orig);
    int result_len = strlen(orig) + count * (with_len - rep_len) + 1;
    char *result = malloc(result_len);
    if (!result) return NULL;
    char *r = result;
    const char *s = orig;
    while (*s) {
        const char *match = strstr(s, rep);
        if (!match) { strcpy(r, s); break; }
        strncpy(r, s, match - s);
        r += match - s;
        strcpy(r, with);
        r += with_len;
        s = match + rep_len;
    }
    return result;
}

void str_truncate(char *s, int max_len) {
    if (!s || max_len < 0) return;
    int len = strlen(s);
    if (len > max_len) {
        if (max_len > 3) {
            s[max_len - 3] = '.';
            s[max_len - 2] = '.';
            s[max_len - 1] = '.';
        }
        s[max_len] = '\0';
    }
}

char *url_encode(const char *s) {
    if (!s) return NULL;
    int len = strlen(s);
    char *out = malloc(len * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    for (int i = 0; i < len; i++) {
        unsigned char c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *p++ = c;
        } else {
            p += sprintf(p, "%%%02X", c);
        }
    }
    *p = '\0';
    return out;
}
