#ifndef JSON_H
#define JSON_H

#include <stdint.h>
#include <stdbool.h>

enum JSON_TYPE {
    JSON_NONE,
    JSON_OBJ,
    JSON_ARR,
    JSON_NUM,
    JSON_STR,
    JSON_BOL,
};

struct json_any_t { 
    enum JSON_TYPE kind; 
    union {
        struct json_obj_t *obj; 
        struct json_arr_t *arr; 
        int64_t           num; 
        char*             str; 
        bool              bol;
    };
};

#define JSON_ANY_NUM(x) ((struct json_any_t){.kind = JSON_NUM, .num = (x)})
#define JSON_ANY_OBJ(x) ((struct json_any_t){.kind = JSON_OBJ, .obj = (x)})
#define JSON_ANY_BOOL(x) ((struct json_any_t){.kind = JSON_BOL, .bol = (x)})

struct kv_pair_t {
    char* key;
    struct json_any_t val;
};


struct json_obj_t {
    struct kv_pair_t* entries; // da
};

struct json_arr_t {
    struct json_any_t *elements;
};

void json_init(char *content);

struct json_obj_t *json_parse();

struct json_any_t json_obj_get(struct json_obj_t *obj, char *key);

void json_obj_put(struct json_obj_t *obj, char *key, struct json_any_t val);

// Str: DA
void json_dumps(char **str, struct json_any_t *json);

#endif // JSON_H
