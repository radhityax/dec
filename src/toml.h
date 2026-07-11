#ifndef TOML_H
#define TOML_H

#include "util.h"

int toml_parse(const char *input, table_t *out);

#endif
