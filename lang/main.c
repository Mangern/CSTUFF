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

#define WALLTIME(t) ((double)(t).tv_sec + 1e-6 * (double)(t).tv_usec)

int assemble_and_link() {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // i am a child
        char * args[] = {"gcc", "tmp.S", NULL};
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

int main(int argc, char** argv) {
    struct timeval t_start, t_end;

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

    create_symbol_tables();

    register_types();

    generate_function_codes();

    if (opt_print_tree) {
        print_tree(root);
    }

    if (opt_print_tac) {
        print_tac();
    }

    gen_outfile = fopen("tmp.S", "w");
    generate_program();
    fclose(gen_outfile);

    if (assemble_and_link()) {
        fprintf(stderr, "Assembler failed\n");
        return EXIT_FAILURE;
    }
    
    gettimeofday ( &t_end, NULL );
    printf ("Total elapsed time: %lf milliseconds\n",
        (WALLTIME(t_end) - WALLTIME(t_start)) * 1000.0
    );

    return 0;
}
