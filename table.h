
#ifndef _TABLE_H_
#define _TABLE_H_

#include "common.h"
#include "value.h"

#define TABLE_MAX_LOAD          (0.75)

typedef struct {
    ObjString *key;
    Value value;
}Entry;

typedef struct {
    int count;
    int capacity;
    Entry *entries;
}Table;

void init_table(Table *table);

void free_table(Table *table);

bool table_set(Table *table, ObjString *key, Value value);

bool table_get(Table *table, ObjString *key, Value *value);

bool table_del(Table *table, ObjString *key);

void table_copy(Table *from, Table *to);

ObjString *table_find_string(Table *table, const char *chars, int length, uint32_t hash);

#endif

