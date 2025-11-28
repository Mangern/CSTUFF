#include <stdlib.h>
#include <string.h>

#include "lex.h"
#include "util.h"
#include "json.h"

json_obj_t* range_to_json(range_t range) {
    json_obj_t *range_obj = calloc(1, sizeof(json_obj_t));
    json_obj_t *start_obj = calloc(1, sizeof(json_obj_t));
    json_obj_t *end_obj   = calloc(1, sizeof(json_obj_t));

    json_obj_put(start_obj, "line", JSON_ANY_NUM(range.start.line));
    json_obj_put(start_obj, "character", JSON_ANY_NUM(range.start.character));
    json_obj_put(end_obj, "line", JSON_ANY_NUM(range.end.line));
    json_obj_put(end_obj, "character", JSON_ANY_NUM(range.end.character));
    json_obj_put(range_obj, "start", JSON_ANY_OBJ(start_obj));
    json_obj_put(range_obj, "end", JSON_ANY_OBJ(end_obj));
    return range_obj;
}

bool uri_eq(char *uri1, char *uri2) {
    if (uri1 == 0 || uri2 == 0) {
        return false;
    }
    // TODO:
    return strcmp(uri1, uri2) == 0;
}
