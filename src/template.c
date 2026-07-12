#define _POSIX_C_SOURCE 200809L

#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} sb_t;

static int sb_init(sb_t *sb, size_t init) {
  sb->buf = malloc(init);
  if (!sb->buf)
    return -1;
  sb->len = 0;
  sb->cap = init;
  return 0;
}

static int sb_ensure(sb_t *sb, size_t extra) {
  size_t needed = sb->len + extra + 1;
  if (needed <= sb->cap)
    return 0;

  size_t newcap = sb->cap * 2;
  while (newcap < needed)
    newcap *= 2;

  char *newbuf = realloc(sb->buf, newcap);
  if (!newbuf)
    return -1;
  sb->buf = newbuf;
  sb->cap = newcap;
  return 0;
}

static int sb_append(sb_t *sb, const char *s, size_t n) {
  if (sb_ensure(sb, n) != 0)
    return -1;
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  return 0;
}

static int sb_append_str(sb_t *sb, const char *s) {
  return sb_append(sb, s, strlen(s));
}

static int append_value(sb_t *sb, value_t *v) {
  switch (v->type) {
  case VAL_STRING:
    return sb_append_str(sb, v->as.str);
  case VAL_INT: {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lld", (long long)v->as.num);
    if (n < 0 || (size_t)n >= sizeof(buf))
      return -1;
    return sb_append(sb, buf, (size_t)n);
  }
  case VAL_BOOL:
    return sb_append_str(sb, v->as.b ? "true" : "false");
  default:
    return 0;
  }
}

static char *extract_field(const char *start, size_t len) {
  while (len > 0 && (*start == ' ' || *start == '\t')) {
    start++;
    len--;
  }
  if (len > 0 && *start == '.') {
    start++;
    len--;
  }
  while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t'))
    len--;

  char *f = malloc(len + 1);
  if (!f)
    return NULL;
  memcpy(f, start, len);
  f[len] = '\0';
  return f;
}

static const char *find_matching_end(const char *p) {
  int depth = 1;
  while (*p) {
    if (p[0] == '{' && p[1] == '{') {
      if (strncmp(p + 2, "range ", 6) == 0) {
        depth++;
        p += 8;
        continue;
      }
      if (strncmp(p + 2, "end}}", 5) == 0) {
        depth--;
        if (depth == 0)
          return p + 7;
        p += 7;
        continue;
      }
    }
    p++;
  }
  return NULL;
}

char *template_render(const char *input, table_t *ctx) {
  sb_t sb;
  if (sb_init(&sb, strlen(input) + 64) != 0)
    return NULL;

  const char *p = input;
  const char *start = input;

  while (*p) {
    if (p[0] == '{' && p[1] == '{') {
      sb_append(&sb, start, (size_t)(p - start));

      if (strncmp(p + 2, "range ", 6) == 0) {
        const char *ts = p + 8;
        const char *te = strstr(ts, "}}");
        if (!te) {
          sb_append_str(&sb, p);
          break;
        }
        char *field = extract_field(ts, (size_t)(te - ts));
        if (!field) {
          free(sb.buf);
          return NULL;
        }
        const char *inner_start = te + 2;
        const char *end_tag = find_matching_end(inner_start);
        if (!end_tag) {
          sb_append_str(&sb, p);
          free(field);
          break;
        }
        size_t inner_len = (size_t)(end_tag - 7 - inner_start);

        value_t *val = table_get(ctx, field);
        if (val && val->type == VAL_ARRAY && val->as.arr) {
          for (size_t i = 0; i < val->as.arr->len; i++) {
            if (val->as.arr->items[i].type != VAL_TABLE)
              continue;
            char *inner = malloc(inner_len + 1);
            if (!inner) {
              free(sb.buf);
              free(field);
              return NULL;
            }
            memcpy(inner, inner_start, inner_len);
            inner[inner_len] = '\0';

            char *rendered =
                template_render(inner, val->as.arr->items[i].as.tbl);
            if (rendered) {
              sb_append_str(&sb, rendered);
              free(rendered);
            }
            free(inner);
          }
        }

        free(field);
        p = end_tag;
        start = p;
        continue;
      }

      if (strncmp(p + 2, "end}}", 5) == 0) {
        p += 7;
        start = p;
        continue;
      }

      const char *te = strstr(p + 2, "}}");
      if (!te) {
        sb_append_str(&sb, p);
        break;
      }

      char *field = extract_field(p + 2, (size_t)(te - (p + 2)));
      if (!field) {
        free(sb.buf);
        return NULL;
      }

      value_t *val = table_get(ctx, field);
      if (val)
        append_value(&sb, val);
      free(field);

      p = te + 2;
      start = p;
      continue;
    }

    p++;
  }

  sb_append(&sb, start, (size_t)(p - start));
  sb.buf[sb.len] = '\0';
  return sb.buf;
}
