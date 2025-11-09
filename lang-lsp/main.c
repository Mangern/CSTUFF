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

void handle_notification(char *method, json_any_t params) {
    LOG("Received notification: %s", method);

    if (strcmp(method, "textDocument/didOpen") == 0) {
        if (params.kind != JSON_OBJ) {
            LOG("Error: missing params");
            return;
        }

        json_obj_t *text_document = json_obj_get(params.obj, "textDocument").obj;

        // parse that shit

        LOG("\"%s\"", json_obj_get(text_document, "text").str);
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
