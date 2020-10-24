/*
 * c-chess-cli, a command line interface for UCI chess engines. Copyright 2020 lucasart.
 *
 * c-chess-cli is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * c-chess-cli is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <pthread.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include "str.h"

// Game results
enum {
    RESULT_LOSS,
    RESULT_DRAW,
    RESULT_WIN,
    NB_RESULT
};

typedef struct {
    // Per engine, by index in engines[] array (not the same as color)
    double sampleFrequency;
    int resignCount, resignScore;
    int drawCount, drawScore;
    bool sampleResolvePv;
    char pad[7];
} GameOptions;

typedef struct {
    pthread_mutex_t mtx;
    int64_t timeLimit;
    str_t engineName;
    bool set;
    char pad[7];
} Deadline;

Deadline deadline_new(void);
void deadline_del(Deadline *d);

// Per thread data
typedef struct {
    Deadline deadline;
    FILE *log;
    uint64_t seed;  // seed for prng()
    int id;  // starts at 1 (0 is for main thread)
    int wldCount[NB_RESULT];  // counts wins, losses, and draws
} Worker;

extern Worker *Workers;
extern _Atomic(int) WorkersBusy;  // how many workers are busy

Worker worker_new(int id, const char *logName);
void worker_del(Worker *w);

void deadline_set(Worker *w, const char *engineName, int64_t timeLimit);
void deadline_clear(Worker *w);
int64_t deadline_overdue(Worker *w);

void workers_add_result(Worker *worker, int result, int wld[3]);

void workers_busy_add(int n);
int workers_busy_count(void);
