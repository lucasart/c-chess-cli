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
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "str.h"

// Engine process
typedef struct {
    FILE *in, *out, *log;
    str_t name;
    pid_t pid;
    int threadId;
} Engine;

// Deadlines overdues are unrecovrable errors. Given a choice, we prefer to handle them gracefully
// as time losses in the worker threads. Since deadlines are enforced by the master thread, there is
// no other choice than to terminate. Any attempt by the master thread to communicate with a buggy
// engine, could result in hanguing forever on blocking I/O.
enum {DEADLINE_TOLERANCE = 1000};

typedef struct {
    pthread_mutex_t mtx;
    const Engine *engine;
    int64_t timeLimit;
} Deadline;

const Engine *deadline_overdue(Deadline *deadline);
Engine engine_new(str_t cmd, str_t name, str_t uciOptions, FILE *log, Deadline *deadline,
    int threadId);
void engine_del(Engine *e);

void engine_readln(const Engine *e, str_t *line);
void engine_writeln(const Engine *e, char *buf);

void engine_sync(const Engine *e, Deadline *deadline);
bool engine_bestmove(const Engine *e, int *score, int64_t *timeLeft, Deadline *deadline,
    str_t *best);

void deadline_set(Deadline *deadline, const Engine *engine, int64_t timeLimit);
void deadline_clear(Deadline *deadline);
