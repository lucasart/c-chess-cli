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
#include "workers.h"
#include "str.h"

typedef struct {
    str_t openings, pgnOut, sampleFileName;
    double elo0, elo1, alpha, beta;
    int concurrency, games, rounds;
    bool log, random, repeat, sprt, gauntlet;
    char pad[7];
} Options;

typedef struct {
    str_t cmd, name, *options;
    int64_t time, increment, movetime, nodes;
    int depth, movestogo;
} EngineOptions;

EngineOptions engine_options_init(void);
void engine_options_destroy(EngineOptions *eo);

Options options_init(void);
void options_parse(int argc, const char **argv, Options *o, GameOptions *go, EngineOptions **eo);
void options_destroy(Options *o);
