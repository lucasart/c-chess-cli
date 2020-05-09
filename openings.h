#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

typedef struct {
    FILE *file;
} Openings;

Openings openings_new(const char *fileName, bool randomStart);
void openings_delete(Openings *openings);

str_t openings_get(Openings *openings);  // FIXME: lock this
