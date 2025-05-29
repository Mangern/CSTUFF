#include <stdio.h>
#include <stdlib.h>

#include "da.h"
#include "lex.h"
#include "parser.h"
#include "symbol.h"
#include "tree.h"

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
    char* filename = "./test-files/hello.lang";
    read_file(filename, &file_content);

    lexer_init(filename, file_content);

    parse();

    create_symbol_tables();

    register_types();

    print_tree(root);
}
