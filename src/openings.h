#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

typedef struct {
    pthread_mutex_t mtx;
    FILE *file;
    size_t count;  // counts the number of calls to openings_new()
    size_t pos;  // next opening at file offset index[pos]
    long *index;  // vector of file offsets
    bool repeat;  // repeat openings (new FEN every 2 calls)
    char pad[7];
} Openings;

Openings openings_new(str_t fileName, bool random, bool repeat, int threadId);
void openings_delete(Openings *openings, int threadId);

int openings_next(Openings *o, str_t *fen, int threadId);
