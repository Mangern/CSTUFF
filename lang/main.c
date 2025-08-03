#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "gen.h"
#include "lex.h"
#include "parser.h"
#include "tac.h"
#include "tree.h"
#include "da.h"
#include "symbol.h"
#include "type.h"

#define BUFSIZE 1024

static bool opt_print_tree = false;
static bool opt_print_tac  = false;

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

static void options(int argc, char **argv) {
    for (;;) {
        switch (getopt(argc, argv, "tpa")) {
            case 't':
                opt_print_tree = true;
                break;
            case 'p':
                opt_print_tac = true;
                break;
            default:
                return;
        }
    }
}

#define WALLTIME(t) (((double)(t).tv_sec + 1e-6 * (double)(t).tv_usec)*1000.0)

int assemble_and_link() {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // i am a child
        char * args[] = {"gcc", "-g", "tmp.S", NULL};
        execvp("gcc", args);

        fprintf(stderr, "execv failed\n");
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "Child terminated with signal %d", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    fprintf(stderr, "Abnormal behavior");
    return 1;
}

void rec(node_t* node) {
    if (node->type_info != NULL) {
        type_print(stdout, node->type_info);
        printf("\n");
    }

    for (size_t i = 0; i < da_size(node->children); ++i) {
        rec(node->children[i]);
    }
}

int main(int argc, char** argv) {
    struct timeval t_start, t_parse, t_create_symbols, t_types, t_ir, t_gen, t_gcc, t_end;

    gettimeofday ( &t_start, NULL );


    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }
    options(argc, argv);

    char* file_content;
    read_file(argv[optind], &file_content);

    lexer_init(argv[optind], file_content);

    parse();

    gettimeofday(&t_parse, NULL);

    create_symbol_tables();

    gettimeofday(&t_create_symbols, NULL);

    register_types();

    gettimeofday(&t_types, NULL);

    generate_function_codes();

    gettimeofday(&t_ir, NULL);

    if (opt_print_tree) {
        print_tree(root);
    }

    if (opt_print_tac) {
        print_tac();
    }

    gen_outfile = fopen("tmp.S", "w");
    generate_program();
    gettimeofday(&t_gen, NULL);
    fclose(gen_outfile);

    if (assemble_and_link()) {
        fprintf(stderr, "Assembler failed\n");
        return EXIT_FAILURE;
    }

    gettimeofday(&t_gcc, NULL);
    
    gettimeofday ( &t_end, NULL );

    printf("==== Execution times ====\n");

    printf("Parser        : %7.3f ms\n", WALLTIME(t_parse) - WALLTIME(t_start));
    printf("Symbol tables : %7.3f ms\n", WALLTIME(t_create_symbols) - WALLTIME(t_parse));
    printf("Type checking : %7.3f ms\n", WALLTIME(t_types) - WALLTIME(t_create_symbols));
    printf("IR gen        : %7.3f ms\n", WALLTIME(t_ir) - WALLTIME(t_types));
    printf("ASM gen       : %7.3f ms\n", WALLTIME(t_gen) - WALLTIME(t_ir));
    printf("GCC           : %7.3f ms\n", WALLTIME(t_gcc) - WALLTIME(t_gen));
    printf("Total time    : %7.3f ms\n", WALLTIME(t_end) - WALLTIME(t_start));

    printf("\nDone compiling %s (%zu bytes)\n", CURRENT_FILE_NAME, da_size(file_content));

    return 0;
}
