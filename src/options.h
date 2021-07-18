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
#include "sprt.h"
#include "str.h"
#include "workers.h"
#include <inttypes.h>

typedef struct {
    str_t fileName;
    double freq, decay;
    bool resolve, bin;
} SampleParams;

typedef struct {
    SampleParams sp;
    str_t openings, pgn;
    SPRTParam sprtParam;
    uint64_t srand;
    int concurrency, games, rounds;
    int resignNumber, resignCount, resignScore;
    int drawNumber, drawCount, drawScore;
    int pgnVerbosity;
    bool log, random, repeat, sprt, gauntlet;
} Options;

typedef struct {
    str_t cmd, name, *vecOptions;
    int64_t time, increment, movetime, nodes, timeOut;
    int depth, movestogo;
} EngineOptions;

EngineOptions engine_options_init(void);
void engine_options_destroy(EngineOptions *eo);

SampleParams sample_params_init(void);
void sample_params_destroy(SampleParams *sp);

Options options_init(void);
EngineOptions *options_parse(int argc, const char **argv, Options *o);
void options_destroy(Options *o);
