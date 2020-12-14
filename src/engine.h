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
#include "workers.h"
#include "options.h"

// Engine process
typedef struct {
    FILE *in, *out;
    str_t name;
    pid_t pid;
    int proto;
    bool supportChess960;
    char pad[3];
} Engine;

// Elements remembered from parsing info lines (for writing PGN comments)
typedef struct {
    int score, depth;
    int64_t time;
} Info;

Engine engine_init(Worker *w, const char *cmd, const char *name, const str_t *options, int proto);
void engine_destroy(Worker *w, Engine *e);

void engine_readln(const Worker *w, const Engine *e, str_t *line);
void engine_writeln(const Worker *w, const Engine *e, char *buf);

void engine_sync(Worker *w, const Engine *e);
bool engine_bestmove(Worker *w, const Engine *e, int64_t *timeLeft, str_t *best, str_t *pv,
    Info *info);
