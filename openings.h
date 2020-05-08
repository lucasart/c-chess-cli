#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

typedef struct {
    FILE *file;
} Openings;

void openings_create(Openings *openings, const char *fileName, bool randomStart);
void openings_destroy(Openings *openings);

str_t openings_get(Openings *openings);
