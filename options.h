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
#include "game.h"
#include "str.h"

typedef struct {
    // options
    int concurrency;  // number of concurrent games
    int games;  // number of games
    str_t openings;  // openings (EPD file)
    str_t pgnout;  // pgn output file
    double elo0, elo1, alpha, beta;  // SPRT parameters
    bool sprt;  // use Sequential Probability Ratio Test

    // flags
    bool random;  // start from a random opening
    bool repeat;  // repeat each opening twice with colors reversed
    bool debug;  // log all I/O with engines

    // engine options
    str_t cmd[2];  // command to execute
    str_t name[2];  // name (to override auto-detect)
    str_t uciOptions[2];  // UCI options per engine (eg. "Hash=16,Threads=8")

    GameOptions go;
} Options;

Options options_new(int argc, const char **argv);
void options_delete(Options *o);
