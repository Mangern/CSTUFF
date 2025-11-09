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
    TYPE_STRING,
    TYPE_SIZE // something like an unsigned int
};

typedef enum {
    TC_BASIC,
    TC_POINTER,
    TC_ARRAY,
    TC_STRUCT,
    TC_STRUCT_FIELD,
    TC_TUPLE,
    TC_FUNCTION,
    TC_TAGGED,
    TC_UNKNOWN
} type_class_t;

struct type_info_t {
    type_class_t type_class;
    union {
        basic_type_t info_basic;
        struct type_array_t* info_array;
        struct type_struct_t* info_struct;
        struct type_struct_field_t* info_struct_field;
        struct type_tuple_t* info_tuple;
        struct type_function_t* info_function;
        struct type_pointer_t* info_pointer;
        struct type_tagged_t* info_tagged;
    } info;
};

struct type_pointer_t {
    type_info_t* inner;
} ;

struct type_array_t {
    type_info_t* subtype;
    size_t* dims;
};

struct type_struct_field_t {
    char* name;
    type_info_t* type;
    size_t offset;
};

struct type_struct_t {
    type_struct_field_t** fields;
};

typedef struct type_tuple_t {
    type_info_t** elems;
} type_tuple_t;

struct type_function_t {
    type_tuple_t* arg_types;
    type_info_t* return_type;
};

typedef struct type_tagged_t {
    size_t type_id;
    type_info_t* type;
} type_tagged_t;

extern char* BASIC_TYPE_NAMES[];
extern symbol_table_t* global_type_table;

void register_types();

type_info_t* type_create_basic(basic_type_t basic_type);

type_info_t* type_penetrate_tagged(type_info_t *type_info);

size_t type_sizeof(type_info_t*);

void type_print(char**, type_info_t*);

#endif // TYPE_H
