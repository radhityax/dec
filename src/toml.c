#define _POSIX_C_SOURCE 200809L

#include "toml.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **p) {
  while (**p && (**p == ' ' || **p == '\t'))
    (*p)++;
}

static void skip_line(const char **p) {
  while (**p && **p != '\n')
    (*p)++;
  if (**p == '\n')
    (*p)++;
}

static int parse_key(const char **p, char *buf, size_t bufsz) {
  skip_ws(p);
  size_t i = 0;
  while (**p && **p != '=' && **p != '\n') {
    if (i < bufsz - 1)
      buf[i++] = **p;
    (*p)++;
  }
  buf[i] = '\0';

  while (i > 0 && (buf[i - 1] == ' ' || buf[i - 1] == '\t'))
    buf[--i] = '\0';

  return (i > 0) ? 0 : -1;
}

static int parse_string(const char **p, char *buf, size_t bufsz) {
  if (**p != '"')
    return -1;
  (*p)++;

  size_t i = 0;
  while (**p && **p != '"') {
    if (**p == '\\' && *(*p + 1)) {
      (*p)++;
      switch (**p) {
      case 'n':
        buf[i++] = '\n';
        break;
      case 't':
        buf[i++] = '\t';
        break;
      case '"':
        buf[i++] = '"';
        break;
      case '\\':
        buf[i++] = '\\';
        break;
      default:
        buf[i++] = **p;
        break;
      }
    } else {
      if (i < bufsz - 1)
        buf[i++] = **p;
    }
    (*p)++;
  }
  buf[i] = '\0';
  if (**p == '"')
    (*p)++;

  return 0;
}

static int parse_int(const char **p, int64_t *out) {
  char *end;
  *out = strtoll(*p, &end, 10);
  if (end == *p)
    return -1;
  *p = end;
  return 0;
}

static int parse_bool(const char **p, bool *out) {
  if (strncmp(*p, "true", 4) == 0) {
    *out = true;
    *p += 4;
    return 0;
  }
  if (strncmp(*p, "false", 5) == 0) {
    *out = false;
    *p += 5;
    return 0;
  }
  return -1;
}

static int parse_unquoted_string(const char **p, char *buf, size_t bufsz) {
  size_t i = 0;
  while (**p && **p != '\n' && **p != ',' && **p != ']' && **p != '}') {
    if (i < bufsz - 1)
      buf[i++] = **p;
    (*p)++;
  }
  buf[i] = '\0';

  while (i > 0 && (buf[i - 1] == ' ' || buf[i - 1] == '\t'))
    buf[--i] = '\0';

  return (i > 0) ? 0 : -1;
}

static int parse_value(const char **p, value_t *val);

static int parse_array(const char **p, value_t *val) {
  if (**p != '[')
    return -1;
  (*p)++;

  array_t *arr = malloc(sizeof(array_t));
  if (!arr)
    return -1;

  arr->cap = 8;
  arr->len = 0;
  arr->items = malloc(arr->cap * sizeof(value_t));
  if (!arr->items) {
    free(arr);
    return -1;
  }

  skip_ws(p);
  while (**p && **p != ']') {
    if (arr->len >= arr->cap) {
      arr->cap *= 2;
      value_t *new_items = realloc(arr->items, arr->cap * sizeof(value_t));
      if (!new_items) {
        free(arr->items);
        free(arr);
        return -1;
      }
      arr->items = new_items;
    }

    if (parse_value(p, &arr->items[arr->len]) != 0) {
      free(arr->items);
      free(arr);
      return -1;
    }
    arr->len++;

    skip_ws(p);
    if (**p == ',') {
      (*p)++;
      skip_ws(p);
    }
  }

  if (**p == ']')
    (*p)++;

  *val = val_array(arr);
  return 0;
}

static int parse_inline_table(const char **p, value_t *val) {
  if (**p != '{')
    return -1;
  (*p)++;

  table_t *tbl = table_new(8);
  if (!tbl)
    return -1;

  skip_ws(p);
  while (**p && **p != '}') {
    char key[256];
    if (parse_key(p, key, sizeof(key)) != 0) {
      table_free(tbl);
      return -1;
    }

    skip_ws(p);
    if (**p != '=') {
      table_free(tbl);
      return -1;
    }
    (*p)++;
    skip_ws(p);

    value_t v;
    if (parse_value(p, &v) != 0) {
      table_free(tbl);
      return -1;
    }

    table_set(tbl, key, v);

    skip_ws(p);
    if (**p == ',') {
      (*p)++;
      skip_ws(p);
    }
  }

  if (**p == '}')
    (*p)++;

  *val = val_table(tbl);
  return 0;
}

static int parse_value(const char **p, value_t *val) {
  skip_ws(p);

  /* quoted string */
  if (**p == '"') {
    char buf[1024];
    if (parse_string(p, buf, sizeof(buf)) != 0)
      return -1;
    *val = val_string(strdup(buf));
    return 0;
  }

  /* array */
  if (**p == '[') {
    return parse_array(p, val);
  }

  /* inline table */
  if (**p == '{') {
    return parse_inline_table(p, val);
  }

  /* boolean */
  if (strncmp(*p, "true", 4) == 0 || strncmp(*p, "false", 5) == 0) {
    bool b;
    if (parse_bool(p, &b) != 0)
      return -1;
    *val = val_bool(b);
    return 0;
  }

  /* integer */
  if (isdigit(**p) || **p == '-' || **p == '+') {
    const char *start = *p;
    int64_t n;
    if (parse_int(p, &n) == 0) {
      /* check if next char is non-digit (like 'T' in date) */
      if (**p && !isspace(**p) && **p != ',' && **p != ']' && **p != '}' &&
          **p != '\n') {
        /* not a pure integer treat as unquoted string (e.g., date) */
        *p = start;
        char buf[256];
        if (parse_unquoted_string(p, buf, sizeof(buf)) != 0)
          return -1;
        *val = val_string(strdup(buf));
        return 0;
      }
      *val = val_int(n);
      return 0;
    }
  }

  /* unquoted string (fallback, handles dates, etc.) */
  char buf[256];
  if (parse_unquoted_string(p, buf, sizeof(buf)) != 0)
    return -1;
  *val = val_string(strdup(buf));
  return 0;
}

int toml_parse(const char *input, table_t *out) {
  const char *p = input;

  while (*p) {
    skip_ws(&p);

    if (*p == '\n') {
      p++;
      continue;
    }
    if (*p == '#') {
      skip_line(&p);
      continue;
    }

    char key[256];
    if (parse_key(&p, key, sizeof(key)) != 0)
      return -1;

    skip_ws(&p);
    if (*p != '=')
      return -1;
    p++;
    skip_ws(&p);

    value_t val;
    if (parse_value(&p, &val) != 0)
      return -1;

    table_set(out, key, val);

    skip_line(&p);
  }

  return 0;
}

#ifdef DEC_TEST
#include <assert.h>

int test_toml(void) {
  table_t *t = table_new(16);
  assert(t);

  const char *input =
      "title = \"my first blog post\"\n"
      "date = 2026-07-09T14:49:00+07:00\n"
      "description = \"a short summary\"\n"
      "draft = false\n"
      "tags = [\"hugo\", \"toml\", \"tutorial\"]\n"
      "categories = [\"hello\"]\n";

  assert(toml_parse(input, t) == 0);

  value_t *v = table_get(t, "title");
  assert(v && v->type == VAL_STRING);
  assert(strcmp(v->as.str, "my first blog post") == 0);

  v = table_get(t, "date");
  assert(v && v->type == VAL_STRING);
  assert(strcmp(v->as.str, "2026-07-09T14:49:00+07:00") == 0);

  v = table_get(t, "draft");
  assert(v && v->type == VAL_BOOL && v->as.b == false);

  v = table_get(t, "tags");
  assert(v && v->type == VAL_ARRAY);
  assert(v->as.arr->len == 3);
  assert(v->as.arr->items[0].type == VAL_STRING);
  assert(strcmp(v->as.arr->items[0].as.str, "hugo") == 0);

  v = table_get(t, "categories");
  assert(v && v->type == VAL_ARRAY);
  assert(v->as.arr->len == 1);

  table_free(t);
  return 0;
}
#endif
