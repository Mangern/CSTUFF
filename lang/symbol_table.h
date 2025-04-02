#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>

#include "langc.h"
#include "symbol.h"

// Stolen from TDT4205

// We use hashmaps to make lookups quick.
// The entries are symbols, using the name of the symbol as the key.
// The hashmap logic is already implemented in symbol_table.c
// NOTE that this hashmap does not support removing entries.
typedef struct symbol_hashmap
{
  symbol_t** buckets; // A bucket may contain 0 or 1 entries
  size_t n_buckets;
  size_t n_entries;

  // If a key is not found, the lookup function will consult this as a backup
  struct symbol_hashmap* backup;
} symbol_hashmap_t;

// A dynamically sized list of symbols, including a hashmap for fast lookups
// The logic for the symbol table is already implemented in symbol_table.c
struct symbol_table_t
{
  symbol_t** symbols;
  size_t n_symbols;
  size_t capacity;
  symbol_hashmap_t* hashmap;
};

typedef enum
{
  INSERT_OK = 0,
  INSERT_COLLISION = 1
} insert_result_t;

// Initializes a new, empty symbol table, including an empty hashmap
symbol_table_t* symbol_table_init(void);

// Tries to insert the given symbol into the symbol table.
// If the topmost hashmap already contains a symbol with the same name,
// INSERT_COLLISION is returned, otherwise the result is INSERT_OK.
//
// The symbol table takes ownership of the symbol, and assigns it a sequence number.
// DO NOT change the symbol's name after insertion.
insert_result_t symbol_table_insert(symbol_table_t* table, symbol_t* symbol);

// Destroys the given symbol table, its hashmap, and all the symbols it owns
void symbol_table_destroy(symbol_table_t* table);

// Initalizes a new, empty hashmap
symbol_hashmap_t* symbol_hashmap_init(void);

// Looks for a symbol in the symbol hashmap, matching the given name.
// If no symbol is found, the hashmap's backup hashmap is checked.
// If the name can't be found in the backup chain either, NULL is returned.
symbol_t* symbol_hashmap_lookup(symbol_hashmap_t* hashmap, const char* name);

// Frees the memory used by the hashmap
void symbol_hashmap_destroy(symbol_hashmap_t* hashmap);

#endif // SYMBOL_TABLE_H
