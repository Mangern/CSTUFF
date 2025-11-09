#ifndef JSON_RPC_H
#define JSON_RPC_H

#include "json.h"

// get the next request
struct json_obj_t *next_request();

void write_response(int64_t id, struct json_any_t result);

#endif // JSON_RPC_H

