#ifndef LANGC_H
#define LANGC_H

#include <stddef.h>

// Forward declarations
typedef struct symbol_table_t symbol_table_t;
typedef struct node_t node_t;
typedef enum symbol_type_t symbol_type_t;
typedef struct symbol_t symbol_t;

typedef enum basic_type_t basic_type_t;
typedef enum type_class_t type_class_t;
typedef struct type_array_t type_array_t;
typedef struct type_struct_t type_struct_t;
typedef struct type_info_t type_info_t;
typedef struct type_named_t type_named_t;
typedef struct type_tuple_t type_tuple_t;
typedef struct type_function_t type_function_t;

typedef enum addr_type_t addr_type_t;
typedef enum instruction_t instruction_t;
typedef struct addr_t addr_t;
typedef struct tac_t tac_t;
typedef struct function_code_t function_code_t;

typedef struct {
    int line;
    int character;
} location_t;

typedef struct {
    location_t start;
    location_t end;
} range_t;

#endif // LANGC_H
