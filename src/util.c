#define _POSIX_C_SOURCE 200809L

#include "util.h"
#include <stdlib.h>
#include <string.h>

table_t *table_new(size_t cap) {
  table_t *t = malloc(sizeof(table_t));
  if (!t)
    return NULL;

  t->items = malloc(sizeof(entry_t) * cap);
  if (!t->items) {
    free(t);
    return NULL;
  }

  t->len = 0;
  t->cap = cap;
  return t;
}

static void value_free(value_t val) {
  if (val.type == VAL_STRING) {
    free(val.as.str);
  } else if (val.type == VAL_ARRAY && val.as.arr) {
    for (size_t j = 0; j < val.as.arr->len; j++)
      value_free(val.as.arr->items[j]);
    free(val.as.arr->items);
    free(val.as.arr);
  } else if (val.type == VAL_TABLE && val.as.tbl) {
    table_free(val.as.tbl);
  }
}

void table_set(table_t *t, const char *key, value_t val) {
  for (size_t i = 0; i < t->len; i++) {
    if (strcmp(t->items[i].key, key) == 0) {
      value_free(t->items[i].val);
      t->items[i].val = val;
      return;
    }
  }

  if (t->len >= t->cap) {
    size_t new_cap = t->cap * 2;
    entry_t *new_items = realloc(t->items, sizeof(entry_t) * new_cap);
    if (!new_items)
      return;
    t->items = new_items;
    t->cap = new_cap;
  }

  t->items[t->len].key = strdup(key);
  t->items[t->len].val = val;
  t->len++;
}

value_t *table_get(table_t *t, const char *key) {
  for (size_t i = 0; i < t->len; i++) {
    if (strcmp(t->items[i].key, key) == 0)
      return &t->items[i].val;
  }
  return NULL;
}

void table_free(table_t *t) {
  if (!t)
    return;

  for (size_t i = 0; i < t->len; i++) {
    free(t->items[i].key);
    value_free(t->items[i].val);
  }

  free(t->items);
  free(t);
}

value_t val_string(char *s) {
  return (value_t){.type = VAL_STRING, .as.str = s};
}

value_t val_int(int64_t n) { return (value_t){.type = VAL_INT, .as.num = n}; }

value_t val_bool(bool b) { return (value_t){.type = VAL_BOOL, .as.b = b}; }

value_t val_table(struct table *tbl) {
  return (value_t){.type = VAL_TABLE, .as.tbl = tbl};
}

value_t val_array(array_t *arr) {
  return (value_t){.type = VAL_ARRAY, .as.arr = arr};
}
