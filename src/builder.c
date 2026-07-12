#define _POSIX_C_SOURCE 200809L

#include "builder.h"
#include "template.h"
#include "toml.h"
#include "util.h"
#include <cmark.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PER_PAGE 6

typedef struct {
  char *title;
  char *date;
  char *url;
  char *content;
} post_t;

static char *read_file(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  if (len < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  char *buf = malloc((size_t)len + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  size_t n = fread(buf, 1, (size_t)len, f);
  fclose(f);
  buf[n] = '\0';

  if (out_len)
    *out_len = n;
  return buf;
}

static int write_file(const char *path, const char *data) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;

  size_t len = strlen(data);
  if (fwrite(data, 1, len, f) != len) {
    fclose(f);
    return -1;
  }

  fclose(f);
  return 0;
}

static int split_fm(const char *buf, char **fm, char **body) {
  *fm = NULL;

  if (strncmp(buf, "+++\n", 4) != 0 && strncmp(buf, "---\n", 4) != 0) {
    *body = strdup(buf);
    return *body ? 0 : -1;
  }

  const char *p = buf + 4;
  const char *end = strstr(p, "\n+++");
  if (!end)
    end = strstr(p, "\n---");

  if (!end) {
    *body = strdup(buf);
    return *body ? 0 : -1;
  }

  size_t flen = (size_t)(end - p);
  if (flen > 0) {
    *fm = malloc(flen + 1);
    if (!*fm)
      return -1;
    memcpy(*fm, p, flen);
    (*fm)[flen] = '\0';
  }

  const char *bs = end + 4;
  if (*bs == '\n')
    bs++;

  *body = strdup(bs);
  if (!*body) {
    free(*fm);
    *fm = NULL;
    return -1;
  }

  return 0;
}

static int cmp_date(const void *a, const void *b) {
  const post_t *pa = *(const post_t **)a;
  const post_t *pb = *(const post_t **)b;
  return strcmp(pb->date, pa->date);
}

static int build_blog(void) {
  DIR *d = opendir("content/blog");
  if (!d)
    return 0;

  post_t **posts = NULL;
  int n = 0, cap = 0;

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    size_t l = strlen(ent->d_name);
    if (l < 3 || strcmp(ent->d_name + l - 3, ".md") != 0)
      continue;

    char path[1024];
    snprintf(path, sizeof(path), "content/blog/%s", ent->d_name);

    char *buf = read_file(path, NULL);
    if (!buf)
      continue;

    char *fm_str = NULL, *body = NULL;
    if (split_fm(buf, &fm_str, &body) != 0) {
      free(buf);
      continue;
    }
    free(buf);

    table_t *ctx = table_new(16);
    if (fm_str) {
      toml_parse(fm_str, ctx);
      free(fm_str);
    }

    char *html = cmark_markdown_to_html(body, strlen(body), CMARK_OPT_DEFAULT);
    if (!html)
      html = strdup("");
    free(body);

    value_t *v_title = table_get(ctx, "title");
    value_t *v_date = table_get(ctx, "date");
    value_t *v_draft = table_get(ctx, "draft");
    if (v_draft && v_draft->type == VAL_BOOL && v_draft->as.b) {
      free(html);
      table_free(ctx);
      continue;
    }

    if (n >= cap) {
      cap = cap ? cap * 2 : 8;
      posts = realloc(posts, sizeof(post_t *) * cap);
      if (!posts) {
        free(html);
        table_free(ctx);
        closedir(d);
        return -1;
      }
    }

    post_t *p = malloc(sizeof(post_t));
    p->title = v_title ? strdup(v_title->as.str) : strdup("Untitled");
    p->date = v_date ? strdup(v_date->as.str) : strdup("");

    size_t bn = l - 3;
    char url_buf[1024];
    snprintf(url_buf, sizeof(url_buf), "blog/%.*s.html", (int)bn, ent->d_name);
    p->url = strdup(url_buf);

    p->content = html;
    posts[n++] = p;

    table_free(ctx);
  }
  closedir(d);

  if (n == 0) {
    free(posts);
    return 0;
  }

  qsort(posts, n, sizeof(post_t *), cmp_date);

  mkdir("public/blog", 0755);

  for (int i = 0; i < n; i++) {
    table_t *ctx = table_new(8);
    table_set(ctx, "title", val_string(strdup(posts[i]->title)));
    table_set(ctx, "date", val_string(strdup(posts[i]->date)));
    table_set(ctx, "content", val_string(strdup(posts[i]->content)));

    char *tpl = read_file("layouts/blog/single.html", NULL);
    if (!tpl)
      tpl = read_file("layouts/_default/single.html", NULL);
    if (!tpl)
      tpl = strdup("<h1>{{.title}}</h1><div>{{.content}}</div>");

    char *out = template_render(tpl, ctx);
    free(tpl);
    table_free(ctx);

    if (out) {
      char outpath[1024];
      snprintf(outpath, sizeof(outpath), "public/%s", posts[i]->url);
      write_file(outpath, out);
      free(out);
    }
  }

  int n_pages = (n + PER_PAGE - 1) / PER_PAGE;

  for (int page = 0; page < n_pages; page++) {
    table_t *ctx = table_new(8);

    int start = page * PER_PAGE;
    int end = start + PER_PAGE;
    if (end > n)
      end = n;
    int count = end - start;

    array_t *arr = malloc(sizeof(array_t));
    arr->items = malloc(sizeof(value_t) * count);
    arr->len = count;
    arr->cap = count;

    for (int i = 0; i < count; i++) {
      table_t *item = table_new(4);
      table_set(item, "title", val_string(strdup(posts[start + i]->title)));
      table_set(item, "date", val_string(strdup(posts[start + i]->date)));
      table_set(item, "url", val_string(strdup(posts[start + i]->url)));
      table_set(item, "content", val_string(strdup(posts[start + i]->content)));
      arr->items[i] = val_table(item);
    }

    char nav[256] = "";
    if (page > 0) {
      char *p = (page == 1) ? "/blog.html" : NULL;
      char buf[64];
      if (!p)
        snprintf(buf, sizeof(buf), "/blog_page_%d.html", page);
      snprintf(nav, sizeof(nav), "<a href=\"%s\">Prev</a>", p ? p : buf);
    }
    if (page < n_pages - 1) {
      size_t off = strlen(nav);
      if (off > 0)
        nav[off++] = ' ';
      snprintf(nav + off, sizeof(nav) - off,
               "<a href=\"/blog_page_%d.html\">Next</a>", page + 2);
    }
    table_set(ctx, "nav", val_string(strdup(nav)));
    table_set(ctx, "posts", val_array(arr));
    table_set(ctx, "page_num", val_int(page + 1));
    table_set(ctx, "total_pages", val_int(n_pages));

    char *tpl = read_file("layouts/blog.html", NULL);
    if (!tpl)
      tpl =
          strdup("{{range .posts}}<h2>{{.title}}</h2><p>{{.date}}</p>{{end}}");

    char *out = template_render(tpl, ctx);
    free(tpl);
    table_free(ctx);

    if (out) {
      char outpath[1024];
      if (page == 0)
        snprintf(outpath, sizeof(outpath), "public/blog.html");
      else
        snprintf(outpath, sizeof(outpath), "public/blog_page_%d.html",
                 page + 1);
      write_file(outpath, out);
      free(out);
    }
  }

  for (int i = 0; i < n; i++) {
    free(posts[i]->title);
    free(posts[i]->date);
    free(posts[i]->url);
    free(posts[i]->content);
    free(posts[i]);
  }
  free(posts);

  printf("blog: %d posts, %d pages\n", n, n_pages);
  return 0;
}

int build_site(void) {
  mkdir("public", 0755);

  build_blog();

  DIR *d = opendir("content");
  if (!d) {
    perror("content");
    return 1;
  }

  struct dirent *ent;
  int n = 0;

  while ((ent = readdir(d)) != NULL) {
    size_t l = strlen(ent->d_name);
    if (l < 3 || strcmp(ent->d_name + l - 3, ".md") != 0)
      continue;

    char inpath[1024];
    snprintf(inpath, sizeof(inpath), "content/%s", ent->d_name);

    char *buf = read_file(inpath, NULL);
    if (!buf) {
      fprintf(stderr, "skip: %s\n", inpath);
      continue;
    }

    char *fm_str = NULL, *body = NULL;
    if (split_fm(buf, &fm_str, &body) != 0) {
      free(buf);
      continue;
    }
    free(buf);

    table_t *ctx = table_new(16);
    if (fm_str) {
      toml_parse(fm_str, ctx);
      free(fm_str);
    }

    char *html = cmark_markdown_to_html(body, strlen(body), CMARK_OPT_DEFAULT);
    if (!html)
      html = strdup("");
    table_set(ctx, "content", val_string(html));
    free(body);

    size_t bn = l - 3;
    char base[512];
    memcpy(base, ent->d_name, bn);
    base[bn] = '\0';

    char tpl_path[1024];
    snprintf(tpl_path, sizeof(tpl_path), "layouts/%s.html", base);
    char *tpl = read_file(tpl_path, NULL);
    if (!tpl) {
      snprintf(tpl_path, sizeof(tpl_path), "layouts/_default/single.html");
      tpl = read_file(tpl_path, NULL);
    }
    if (!tpl)
      tpl = strdup("{{.content}}");

    char *out = template_render(tpl, ctx);
    free(tpl);
    table_free(ctx);
    if (!out) {
      fprintf(stderr, "render failed: %s\n", ent->d_name);
      continue;
    }

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "public/%s.html", base);
    if (write_file(outpath, out) != 0)
      fprintf(stderr, "write failed: %s\n", outpath);
    else
      n++;

    free(out);
  }

  closedir(d);
  printf("flat: %d pages\n", n);
  return 0;
}
