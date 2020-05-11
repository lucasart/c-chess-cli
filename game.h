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
#include "position.h"
#include "engine.h"

enum {
    RESULT_NONE,
    RESULT_CHECKMATE,  // lost by being checkmated
    RESULT_STALEMATE,  // draw by stalemate
    RESULT_THREEFOLD,  // draw by 3 position repetition
    RESULT_FIFTY_MOVES,  // draw by 50 moves rule
    RESULT_INSUFFICIENT_MATERIAL,  // draw due to insufficient material to deliver checkmate
    RESULT_ILLEGAL_MOVE  // lost by playing an illegal move
};

typedef struct {
    str_t names[NB_COLOR];  // names of players, by color
    Position *pos;  // list of positions (including moves) since game start
    uint64_t nodes[2];  // node limit per move (per player, by order)
    int depth[2];  // depth limit per move (per player, by order)
    int movetime[2];  // time limit per move (per player, by order)
    int ply, maxPly;
    bool chess960;
    int result;
} Game;

Game game_new(bool chess960, const char *fen, uint64_t nodes[2], int depth[2], int movetime[2]);
void game_delete(Game *g);

void game_play(Game *g, const Engine *first, const Engine *second);
str_t game_decode_result(const Game *g, str_t *reason);
str_t game_pgn(const Game *g);
