#include "symbol_table.h"
#include "assert.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static insert_result_t symbol_hashmap_insert(symbol_hashmap_t* hashmap, symbol_t* symbol);

// ================== Symbol table code =================
// Initializes a symboltable with 0 entries. Will be resized upon first insertion
symbol_table_t* symbol_table_init(void)
{
  symbol_table_t* result = malloc(sizeof(symbol_table_t));
  *result = (symbol_table_t){.symbols = NULL,
                             .n_symbols = 0,
                             .capacity = 0,
                             .hashmap = symbol_hashmap_init()};
  return result;
}

// Adds a symbol to both the symbol table, and its hashmap (if possible)
insert_result_t symbol_table_insert(symbol_table_t* table, symbol_t* symbol)
{
  // Inserts can fail, if the hashmap already contains the name
  if (symbol_hashmap_insert(table->hashmap, symbol) == INSERT_COLLISION)
    return INSERT_COLLISION;

  // If the table is full, resize the list
  if (table->n_symbols + 1 >= table->capacity)
  {
    table->capacity = table->capacity * 2 + 8;
    table->symbols = realloc(table->symbols, table->capacity * sizeof(symbol_t*));
  }

  table->symbols[table->n_symbols] = symbol;
  symbol->sequence_number = table->n_symbols;
  table->n_symbols++;

  return INSERT_OK;
}

// Destroys the given symbol table, its hashmap, and all the symbols it owns
void symbol_table_destroy(symbol_table_t* table)
{
  for (size_t i = 0; i < table->n_symbols; i++)
    free(table->symbols[i]);
  free(table->symbols);
  symbol_hashmap_destroy(table->hashmap);
  free(table);
}

// ==================== Hashmap code ====================

// Initializes a hashmap with 0 buckets. Will be resized upon first insertion
symbol_hashmap_t* symbol_hashmap_init()
{
  symbol_hashmap_t* result = malloc(sizeof(symbol_hashmap_t));
  *result = (symbol_hashmap_t){.buckets = NULL, .n_buckets = 0, .n_entries = 0, .backup = NULL};
  return result;
}

// Calculates a naive 64-bit hash of the given string
static uint64_t hash_string(const char* string)
{
  assert(string != NULL);
  uint64_t hash = 31;
  for (const char* c = string; *c != '\0'; c++)
    hash = hash * 257 + *c;
  return hash;
}

// Allocates a larger list of buckets, and inserts all hashmap entries again
static void symbol_hashmap_resize(symbol_hashmap_t* hashmap, size_t new_capacity)
{
  symbol_t** old_buckets = hashmap->buckets;
  size_t old_capacity = hashmap->n_buckets;

  // Use calloc, since it initalizes the memory to 0, aka NULL entries
  hashmap->buckets = calloc(new_capacity, sizeof(symbol_t*));
  hashmap->n_buckets = new_capacity;
  hashmap->n_entries = 0;

  // Now re-insert all entries from the old buckets
  for (size_t i = 0; i < old_capacity; i++)
  {
    if (old_buckets[i] != NULL)
      symbol_hashmap_insert(hashmap, old_buckets[i]);
  }

  free(old_buckets);
}

// Performs insertion into the hashmap.
// The hashmap uses open addressing, with up to one entry per bucket.
// If our first choice of bucket is full, we look at the next bucket, until we find room.
static insert_result_t symbol_hashmap_insert(symbol_hashmap_t* hashmap, symbol_t* symbol)
{
  // Make sure that the fill ratio of the hashmap never exeeds 1/2
  size_t new_size = hashmap->n_entries + 1;
  if (new_size * 2 > hashmap->n_buckets)
    symbol_hashmap_resize(hashmap, hashmap->n_buckets * 2 + 8);

  // Now calculate the position of the new entry
  uint64_t hash = hash_string(symbol->name);
  size_t bucket = hash % hashmap->n_buckets;

  // Iterate until we find an empty bucket
  while (hashmap->buckets[bucket] != NULL)
  {
    // Check if the existing entry is a name collision
    if (strcmp(hashmap->buckets[bucket]->name, symbol->name) == 0)
      return INSERT_COLLISION; // An entry with the same name already exists
    // Go to the next bucket
    bucket = (bucket + 1) % hashmap->n_buckets;
  }

  // We found an emoty bucket, insert the symbol here
  hashmap->buckets[bucket] = symbol;
  hashmap->n_entries++;
  return INSERT_OK; // We successfully inserted a new symbol
}

// Performs lookup in the hashmap.
// Hashes the given string, and checks if the resulting bucket contains the item.
// Since the hashmap uses open addressing, the entry can also be in the next bucket,
// so we iterate until we either find the item, or find an empty bucket.
//
// If the key isn't found in this hashmap, but we have a backup, lookup continues there.
// Otherwise, NULL is returned.
symbol_t* symbol_hashmap_lookup(symbol_hashmap_t* hashmap, const char* name)
{
  uint64_t hash = hash_string(name);

  // Loop through the linked list of hashmaps and backup hashmaps
  while (hashmap != NULL)
  {
    // Skip any hashmaps with 0 buckets
    if (hashmap->n_buckets == 0)
    {
      hashmap = hashmap->backup;
      continue;
    }

    size_t bucket = hash % hashmap->n_buckets;
    while (hashmap->buckets[bucket] != NULL)
    {
      // Check if the entry in the bucket has a matching name
      if (strcmp(hashmap->buckets[bucket]->name, name) == 0)
        return hashmap->buckets[bucket];

      // Otherwise keep iterating until we find a hit, or an empty bucket
      bucket = (bucket + 1) % hashmap->n_buckets;
    }

    // No entry with the required name existed in the hashmap, so go to the backup
    hashmap = hashmap->backup;
  }

  // The entry was never found, and we are all out of backups
  return NULL;
}

void symbol_hashmap_destroy(symbol_hashmap_t* hashmap)
{
  free(hashmap->buckets);
  free(hashmap);
}

