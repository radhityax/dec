#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
  VAL_STRING,
  VAL_INT,
  VAL_BOOL,
  VAL_ARRAY,
  VAL_TABLE
} val_type_t;

typedef struct value {
  val_type_t type;
  union {
    char *str;
    int64_t num;
    bool b;
    struct array *arr;
    struct table *tbl;
  } as;
} value_t;

typedef struct array {
  value_t *items;
  size_t len;
  size_t cap;
} array_t;

typedef struct entry {
  char *key;
  value_t val;
} entry_t;

typedef struct table {
  entry_t *items;
  size_t len;
  size_t cap;
} table_t;

table_t *table_new(size_t cap);
void table_set(table_t *t, const char *key, value_t val);
value_t *table_get(table_t *t, const char *key);
void table_free(table_t *t);

value_t val_string(char *s);
value_t val_int(int64_t n);
value_t val_bool(bool b);
value_t val_table(struct table *tbl);
value_t val_array(array_t *arr);

#endif
