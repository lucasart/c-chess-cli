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
    int concurrency, games;
    bool log, random, repeat, sprt;
    char pad[4];
} Options;

Options options_new(void);
void options_parse(int argc, const char **argv, Options *o, GameOptions *go, EngineOptions **eo);
void options_del(Options *o);
