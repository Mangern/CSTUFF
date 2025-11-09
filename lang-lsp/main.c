#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "json.h"
#include "json_types.h"
#include "json_rpc.h"
#include "da.h"

#include "lex.h"
#include "parser.h"
#include "tree.h"
#include "fail.h"

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

void publish_diagnostics(char *uri) {
    json_obj_t msg = {0};
    json_obj_put(&msg, "uri", JSON_ANY_STR(uri));
    json_arr_t diags = {0};

    for (size_t i = 0; i < da_size(diagnostics); ++i) {
        json_obj_t *diag_obj = calloc(1, sizeof(json_obj_t));
        json_obj_put(diag_obj, "range", JSON_ANY_OBJ(range_to_json(diagnostics[i].range)));
        json_obj_put(diag_obj, "message", JSON_ANY_STR(diagnostics[i].message));
        da_append(diags.elements, JSON_ANY_OBJ(diag_obj));
    }

    json_obj_put(&msg, "diagnostics", JSON_ANY_ARR(&diags));

    json_any_t params;
    params.kind = JSON_OBJ;
    params.obj = &msg;

    write_notification("textDocument/publishDiagnostics", params);

    // TODO: free memory or something
}

void handle_notification(char *method, json_any_t params) {
    LOG("Received notification: %s", method);

    if (strcmp(method, "textDocument/didOpen") == 0) {
        if (params.kind != JSON_OBJ) {
            LOG("Error: missing params");
            return;
        }

        json_obj_t *text_document = json_obj_get(params.obj, "textDocument").obj;

        // parse that shit
        char *uri = json_obj_get(text_document, "uri").str;
        char *content = json_obj_get(text_document, "text").str;
        size_t content_len = strlen(content);

        // lexer wants content to be a da.
        // we also need to convert escaped stuff into the normal stuff
        char *content_da = {0};
        da_reserve(content_da, content_len);

        for (size_t i = 0; i < content_len; ++i) {
            if (i < content_len - 1 && content[i] == '\\') {
                switch(content[i+1]) {
                    case 'n':
                        da_append(content_da, '\n');
                        ++i;
                        break;
                    case '"':
                        da_append(content_da, '"');
                        ++i;
                        break;
                    default:
                        LOG("Unhandled escape char %c", content[i+1]);
                        exit(1);
                }
            } else {
                da_append(content_da, content[i]);
            }
        }

        lexer_init(uri, content_da);

        int err = setjmp(FAIL_JMP_ENV);

        if (err) {
            // returned from longjmp
            publish_diagnostics(uri);
            return;
        }

        parse();
        print_tree(LOG_FILE, root);

        fflush(LOG_FILE);

    }
}

void handle_request(int64_t id, char *method, json_any_t params) {
    LOG("Received request: %s", method);

    if (strcmp(method, "initialize") == 0) {
        if (params.kind != JSON_OBJ) {
            LOG("Error: missing params");
            return;
        }
        json_obj_t capabilities = {0};
        // textDocumentSync == 1 == TextDocumentSyncKind.Full
        json_obj_put(&capabilities, "textDocumentSync", JSON_ANY_NUM(1));
        json_obj_t result = {0};
        json_obj_put(&result, "capabilities", JSON_ANY_OBJ(&capabilities));
        write_response(id, JSON_ANY_OBJ(&result));
        return;
    }
}

int main() {
    setvbuf(stdin, NULL, _IONBF, 0);
    log_init();
    fail_init_diagnostic();

    for (;;) {
        json_obj_t *obj = next_request();

        char *method;
        int64_t id;
        json_any_t params;

        {
            json_any_t method_val = json_obj_get(obj, "method");
            if (method_val.kind != JSON_STR) {
                LOG("Expected method to be str, but was %d", method_val.kind);
                exit(1);
            }
            method = method_val.str;
        }
        {
            json_any_t id_val = json_obj_get(obj, "id");
            if (id_val.kind != JSON_NUM && id_val.kind != JSON_NONE) {
                LOG("Expected id to be int, but was %d", id_val.kind);
                LOG("%s", method);
                exit(1);
            }
            id = id_val.kind == JSON_NUM ? id_val.num : 0;
        }

        {
            params = json_obj_get(obj, "params");

            // Three valid types
            if (params.kind != JSON_NONE && params.kind != JSON_OBJ && params.kind != JSON_ARR) {
                LOG("Invalid params type %d", params.kind);
                exit(1);
            }
        }

        if (id == 0) {
            // TODO: process?
            handle_notification(method, params);
            continue;
        }
        handle_request(id, method, params);

    }

    fclose(LOG_FILE);
    return 1;
}
