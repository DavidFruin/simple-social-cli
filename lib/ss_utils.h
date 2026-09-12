#ifndef SS_UTILS_H
#define SS_UTILS_H

#include <stddef.h>

char *str_trim(char *s);
char *str_dup(const char *s);
char *str_lower(const char *s);
int str_starts_with(const char *s, const char *prefix);
char *str_replace(const char *orig, const char *rep, const char *with);
void str_truncate(char *s, int max_len);
char *url_encode(const char *s);

#endif
