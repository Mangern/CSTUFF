#ifndef SEMANTIC_TOKENS_H
#define SEMANTIC_TOKENS_H

#include <stdint.h>

#include "json_types.h"
#include "tree.h"

json_obj_t* get_semantic_tokens_options();

void handle_semantic_tokens(int64_t request_id, node_t *root);

#endif // SEMANTIC_TOKENS_H
