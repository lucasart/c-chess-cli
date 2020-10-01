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
#include "engine.h"

// Game results
enum {
    RESULT_LOSS,
    RESULT_DRAW,
    RESULT_WIN,
    NB_RESULT
};

typedef struct {
    str_t cmd, name, uciOptions;
    int64_t movetime, nodes;
    int depth;
    char pad[4];
} EngineOptions;

typedef struct {
    // Per engine, by index in engines[] array (not the same as color)
    int64_t time[2], increment[2];
    double sampleFrequency;
    int movestogo[2];
    int resignCount, resignScore;
    int drawCount, drawScore;
    bool sampleResolvePv;
    char pad[7];
} GameOptions;

// Per thread data
typedef struct {
    Deadline deadline;
    FILE *pgnOut;
    FILE *sampleFile;
    const GameOptions *go;
    uint64_t seed;  // seed for prng()
    int id;  // starts at 1 (0 is for main thread)
    int wldCount[NB_RESULT];  // counts wins, losses, and draws
} Worker;

extern Worker *Workers;
extern _Atomic(int) WorkersBusy;  // how many workers are busy

void workers_new(int count, FILE *pgnOut, FILE *sampleFile, const GameOptions *go);
void workers_delete(void);

void workers_add_result(Worker *worker, int result, int wld[3]);

void workers_busy_add(int n);
int workers_busy_count(void);
