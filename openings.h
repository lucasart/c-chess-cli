#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

typedef struct {
    pthread_mutex_t mtx;
    FILE *file;
    str_t lastFen;
    int next;
    bool repeat;
} Openings;

Openings openings_new(const char *fileName, bool random, int repeat);
void openings_delete(Openings *openings);

int openings_next(Openings *o, str_t *fen);
