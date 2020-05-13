#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

typedef struct {
    FILE *file;
    int roundsLeft;
    pthread_mutex_t mtx;
} Openings;

Openings openings_new(const char *fileName, int rounds, bool random);
void openings_delete(Openings *openings);

bool openings_get(Openings *o, str_t *fen);
