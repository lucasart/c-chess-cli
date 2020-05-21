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
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "str.h"

// Engine process
typedef struct {
    pid_t pid;
    FILE *in, *out, *log;
    str_t name;
} Engine;

Engine engine_create(const char *cmd, FILE *log, const char *uciOptions);
void engine_delete(Engine *e);

void engine_readln(const Engine *e, str_t *line);
void engine_writeln(const Engine *e, char *buf);

void engine_sync(const Engine *e);
str_t engine_bestmove(const Engine *e, int *score, int64_t *timeLeft);
