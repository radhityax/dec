#include "builder.h"
#include <stdio.h>
#include <string.h>

#define VERSION "0.1.0"

static void print_usage(const char *prog) {
  printf("dec %s - static site generator\n\n", VERSION);
  printf("usage: %s <command> [options]\n\n", prog);
  printf("commands:\n");
  printf("build - build site to public/\n");
  printf("version - print version\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "version") == 0) {
    printf("dec %s\n", VERSION);
    return 0;
  }

  if (strcmp(cmd, "build") == 0) {
    return build_site();
  }

  fprintf(stderr, "unknown command: %s\n", cmd);
  print_usage(argv[0]);
  return 1;
}
