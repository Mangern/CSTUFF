#ifndef TYPE_H
#define TYPE_H

#include "langc.h"

enum basic_type_t {
    TYPE_VOID,
    TYPE_INT,
    TYPE_REAL,
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_STRING
};

enum type_class_t {
    TC_BASIC,
    TC_ARRAY,
    TC_STRUCT,
    TC_TUPLE,
    TC_FUNCTION
};

struct type_info_t {
    type_class_t type_class;
    union {
        type_array_t* info_array;
        basic_type_t info_basic;
        type_struct_t* info_struct;
        type_tuple_t* info_tuple;
        type_function_t* info_function;
    } info;
};

struct type_array_t {
    size_t count;
    type_info_t* subtype;
};

struct type_named_t {
    char* name;
    type_info_t* type;
};

struct type_struct_t {
    type_named_t** fields;
};

struct type_tuple_t {
    type_info_t** elems;
};

struct type_function_t {
    type_tuple_t* arg_types;
    type_info_t* return_type;
};

void register_types();

#endif // TYPE_H
