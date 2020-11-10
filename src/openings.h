#pragma once
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

typedef struct {
    pthread_mutex_t mtx;
    FILE *file;
    long *index;  // vector of file offsets
} Openings;

Openings openings_init(const char *fileName, bool random, uint64_t srand, int threadId);
void openings_destroy(Openings *openings, int threadId);

void openings_next(Openings *o, str_t *fen, size_t idx, int threadId);
