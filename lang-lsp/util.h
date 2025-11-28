#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

#include "lex.h"
#include "json_types.h"

json_obj_t* range_to_json(range_t range);

bool uri_eq(char *uri1, char *uri2);

#endif // UTIL_H
