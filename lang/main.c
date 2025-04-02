#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include "interpret.h"
#include "lex.h"
#include "parser.h"
#include "tree.h"
#include "da.h"
#include "symbol.h"
#include "type.h"

#define BUFSIZE 1024

static bool opt_print_tree = false;
static bool opt_interpret  = false;

void read_file(const char* file_path, char** content) {
    FILE* file = fopen(file_path, "r");

    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        exit(1);
    }

    *content = da_init(char);

    char buffer[BUFSIZE];
    for (;;) {
        size_t num_read = fread(buffer, 1, BUFSIZE, file);
        if (num_read == 0) break;

        for (size_t i = 0; i < num_read; ++i) {
            da_append(content, buffer[i]);
        }
    }

    fclose(file);
}

static void options(int argc, char **argv) {
    for (;;) {
        switch (getopt(argc, argv, "ti")) {
            case 't':
                opt_print_tree = true;
                break;
            case 'i':
                opt_interpret = true;
                break;
            default:
                return;
        }
    }
}


int main(int argc, char** argv) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }
    options(argc, argv);

    char* file_content;
    read_file(argv[optind], &file_content);

    lexer_init(argv[optind], file_content);

    parse();

    create_symbol_tables();

    register_types();


    if (opt_print_tree) {
        print_tree(root);
    }

    if (opt_interpret) {
        interpret_root();
    }


    return 0;
}
