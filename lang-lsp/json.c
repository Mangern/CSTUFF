#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "json.h"
#include "json_types.h"
#include "da.h"
#include "log.h"


/* ------------------------------ Parsing ------------------------------ */

static char *content;
static ssize_t content_ptr;
static ssize_t content_len;
static char cur_char;
static ssize_t cur_idx;

static char peek();
static void advance();
static char peek_expect_advance(char tok);

static json_any_t  parse_any();
static json_obj_t* parse_object();
static json_arr_t* parse_array();
static char*       parse_string();
static int64_t     parse_number();
static bool        parse_bool();

void json_init(char *json_content) {
    content = json_content;
    content_len = da_size(json_content);
    content_ptr = 0;
    cur_idx = -1;
}

json_obj_t* json_parse() {
    json_obj_t *obj = parse_object();
    peek_expect_advance(EOF);
    return obj;
}

json_obj_t* parse_object() {
    peek_expect_advance('{');
    json_obj_t *obj = calloc(1, sizeof(json_obj_t));

    char tok = peek();
    if (tok == '}') {
        advance();
        return obj;
    }
    for (;;) {
        tok = peek();

        if (tok == '"') {
            char *key = parse_string();
            peek_expect_advance(':');

            json_any_t value = parse_any();

            json_obj_put(obj, key, value);

            tok = peek();
            if (tok == ',') {
                advance();
                continue;
            } else {
                break;
            }
        } else {
            LOG("Unexpected token in JSON object: %c", tok);
            exit(1);
        }
    }
    peek_expect_advance('}');
    return obj;
}

json_arr_t* parse_array() {
    peek_expect_advance('[');
    json_arr_t *arr = calloc(1, sizeof(json_arr_t));

    char tok = peek();

    if (tok == ']') {
        advance();
        return arr;
    }

    for (;;) {
        char tok = peek();

        json_any_t el = parse_any();
        da_append(arr->elements, el);

        tok = peek();

        if (tok == ',') {
            advance();
            continue;
        } else break;
    }
    peek_expect_advance(']');
    return arr;
}

json_any_t parse_any() {
    json_any_t ret;

    char tok = peek();

    if (tok == '"') {
        ret.kind = JSON_STR;
        ret.str = parse_string();
        return ret;
    }

    if (tok == '{' ) {
        ret.kind = JSON_OBJ;
        ret.obj = parse_object();
        return ret;
    }

    if (tok == '[') {
        ret.kind = JSON_ARR;
        ret.arr = parse_array();
        return ret;
    }

    if (isdigit(tok)) {
        ret.kind = JSON_NUM;
        ret.num = parse_number();
        return ret;
    }

    if (tok == 't' || tok == 'f') {
        ret.kind = JSON_BOL;
        ret.bol = parse_bool();
        return ret;
    }

    LOG("JSON parse error: Unexpected token %c", tok);
    exit(1);
}

char* parse_string() {
    peek_expect_advance('"');
    char tok;

    size_t start = content_ptr;

    for (;;) {
        // TODO: escaping
        tok = peek();
        advance();

        if (tok == '\\') {
            advance();
            continue;
        }

        if (tok == '"') {
            break;
        }
    }

    return strndup(&content[start], content_ptr - start - 1);
}

int64_t parse_number() {
    // TODO: negative number? float?
    int64_t ret = 0;
    for (;;) {
        char tok = peek();

        if (!isdigit(tok)) break;

        ret = 10 * ret + (tok - '0');

        advance();
    }

    return ret;
}

bool parse_bool() {
    char tok = peek();

    if (tok == 't') {
        peek_expect_advance('t');
        peek_expect_advance('r');
        peek_expect_advance('u');
        peek_expect_advance('e');
        return true;
    } else {
        peek_expect_advance('f');
        peek_expect_advance('a');
        peek_expect_advance('l');
        peek_expect_advance('s');
        peek_expect_advance('e');
        return false;
    }
}

char peek() {
    if (cur_idx == content_ptr) {
        return cur_char;
    }

    // skip whitespace
    while (content_ptr < content_len && isspace(content[content_ptr])) {
        ++content_ptr;
    }

    if (content_ptr >= content_len) {
        cur_char = EOF;
        cur_idx = content_ptr;
        return cur_char;
    }

    cur_idx = content_ptr;
    cur_char = content[content_ptr];
    return cur_char;
}

void advance() {
    content_ptr++;
    peek();
}

char peek_expect_advance(char tok) {
    char ret = peek();
    if (ret != tok) {
        LOG("JSON parse error: Expected '%c', got '%c'", tok, ret);
        exit(1);
    }
    advance();
    return ret;
}

/* ------------------------------ JSON interface ------------------------------ */

// TODO: trie or hashmap if necessary
json_any_t json_obj_get(struct json_obj_t *obj, char *key) {
    for (size_t i = 0; i < da_size(obj->entries); ++i) {
        if (strcmp(key, obj->entries[i].key) == 0) {
            return obj->entries[i].val;
        }
    }

    return (json_any_t){.kind = JSON_NONE };
}

void json_obj_put(struct json_obj_t *obj, char *key, struct json_any_t val) {
    struct kv_pair_t kv = {
        .key = key,
        .val = val
    };
    da_append(obj->entries, kv);
}

void json_dumps(char **str, struct json_any_t json) {
    switch(json.kind) {
    case JSON_NONE:
        {
            da_strncat(str, "null", 4);
        }
        break;
    case JSON_OBJ:
        {
            json_obj_t *obj = json.obj;
            da_append(*str, '{');

            for (size_t i = 0; i < da_size(obj->entries); ++i) {
                if (i > 0) {
                    da_append(*str, ',');
                }
                char *key = obj->entries[i].key;
                da_append(*str, '"');
                da_strncat(str, key, strlen(key));
                da_append(*str, '"');
                da_append(*str, ':');
                json_dumps(str, obj->entries[i].val);
            }
            da_append(*str, '}');
        }
        break;
    case JSON_ARR:
        {
            json_arr_t *arr = json.arr;
            da_append(*str, '[');

            for (size_t i = 0; i < da_size(arr->elements); ++i) {
                if (i > 0) {
                    da_append(*str, ',');
                }

                json_dumps(str, arr->elements[i]);
            }
            da_append(*str, ']');
        }
        break;
    case JSON_NUM:
        {
            // uuh
            char buf[16];
            memset(buf, 0, sizeof buf);
            sprintf(buf, "%ld", json.num);
            da_strncat(str, buf, strlen(buf));
        }
        break;
    case JSON_STR:
        {
            da_append(*str, '"');
            da_strncat(str, json.str, strlen(json.str));
            da_append(*str, '"');
        }
        break;
    case JSON_BOL:
        {
            if (json.bol == true) {
                da_strncat(str, "true", 4);
            } else {
                da_strncat(str, "false", 5);
            }
        }
        break;
    }
}
