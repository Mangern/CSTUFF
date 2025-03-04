#include "regex_gen.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

char* generate_regex(int depth) {
    if (depth == 0) {
        char* res = malloc(2);
        res[0] = 'a' + rand() % 26;
        res[1] = 0;
        return res;
    }

    int rule = rand() % 5;
    switch(rule) {
        case 0: {
            char* sub = generate_regex(depth - 1);
            char* ret = malloc(3 + strlen(sub));
            ret[0] = '(';
            ret[1] = 0;
            strcat(ret, sub);
            free(sub);
            char* suf = ")";
            strcat(ret, suf);
            return ret;
        }
        case 1: {
            char* sub = generate_regex(depth - 1);
            sub = realloc(sub, strlen(sub) + 2);
            char* suf = "*";
            strcat(sub, suf);
            return sub;
        }
        case 2: {
            char* sub = generate_regex(depth - 1);
            sub = realloc(sub, strlen(sub) + 2);
            char* suf = "+";
            strcat(sub, suf);
            return sub;
        }
        case 3: {
            char* sub = generate_regex(depth - 1);
            sub = realloc(sub, strlen(sub) + 2);
            char* suf = "?";
            strcat(sub, suf);
            return sub;
        }
        case 4: {
            char* sub1 = generate_regex(depth - 1);
            char* sub2 = generate_regex(depth - 1);
            int sub1_length = strlen(sub1);
            sub1 = realloc(sub1, strlen(sub1) + strlen(sub2) + 2);
            sub1[sub1_length] = '|';
            sub1[sub1_length+1] = 0;
            strcat(sub1, sub2);
            free(sub2);
            return sub1;
        }
    }
    assert(0);
}

int main() {
    srand(time(NULL));
    char* regex = generate_regex(5);
    printf("%s\n", regex);
    free(regex);
    return 0;
}
