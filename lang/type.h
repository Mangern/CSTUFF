#ifndef TYPE_H
#define TYPE_H

#include <stdio.h>

#include "langc.h"

enum basic_type_t {
    TYPE_VOID,
    TYPE_INT,
    TYPE_REAL,
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_STRING
};

typedef enum {
    TC_BASIC,
    TC_ARRAY,
    TC_STRUCT,
    TC_TUPLE,
    TC_FUNCTION,
    TC_UNKNOWN
} type_class_t;

struct type_info_t {
    type_class_t type_class;
    union {
        type_array_t* info_array;
        basic_type_t info_basic;
        type_struct_t* info_struct;
        struct type_tuple_t* info_tuple;
        type_function_t* info_function;
    } info;
};

struct type_array_t {
    type_info_t* subtype;
    size_t* dims;
};

struct type_named_t {
    char* name;
    type_info_t* type;
};

struct type_struct_t {
    type_named_t** fields;
};

typedef struct type_tuple_t {
    type_info_t** elems;
} type_tuple_t;

struct type_function_t {
    type_tuple_t* arg_types;
    type_info_t* return_type;
};

extern char* BASIC_TYPE_NAMES[];

void register_types();

void type_print(FILE*, type_info_t*);

#endif // TYPE_H
