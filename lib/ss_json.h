#ifndef SS_JSON_H
#define SS_JSON_H

#include <stddef.h>

typedef struct {
    const char *start;
    const char *end;
    int type; /* 0=string, 1=number, 2=bool/null, 3=object, 4=array */
} json_value;

typedef struct {
    const char *key;
    json_value val;
} json_field;

int json_get_string(const char *json, const char *key, char *out, int out_size);
int json_get_int(const char *json, const char *key, int *out);
int json_get_bool(const char *json, const char *key, int *out);
int json_get_array(const char *json, const char *key, const char **start, const char **end);
int json_array_len(const char *arr_start, const char *arr_end);
int json_array_get_item(const char *arr_start, const char *arr_end, int index,
                        const char **item_start, const char **item_end);
int json_get_nested(const char *json, const char *key, const char **start, const char **end);

#endif
