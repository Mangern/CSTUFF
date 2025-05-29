#include <stdio.h>
#include "da.h"
#include "lex.h"

#define BUFSIZE 1024
void read_file(const char* file_path, char** content) {
    FILE* file = fopen(file_path, "r");

    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        exit(1);
    }

    *content = 0;

    char buffer[BUFSIZE];
    for (;;) {
        size_t num_read = fread(buffer, 1, BUFSIZE, file);
        if (num_read == 0) break;

        for (size_t i = 0; i < num_read; ++i) {
            da_append(*content, buffer[i]);
        }
    }

    fclose(file);
}

int main() {
    char* file_content;
    char* filename = "./test-files/euler3.lang";
    read_file(filename, &file_content);

    lexer_init(filename, file_content);

    for (;;) {
        token_t token = lexer_peek();

        printf("%s\n", TOKEN_TYPE_NAMES[token.type]);

        if (token.type == LEX_END) break;

        lexer_advance();
    }
}
