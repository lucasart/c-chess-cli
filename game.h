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
    RESULT_ILLEGAL_MOVE,  // lost by playing an illegal move
    RESULT_DRAW_ADJUDICATION,  // draw by adjudication
    RESULT_RESIGN,  // resigned on behalf of the engine
    RESULT_TIME_LOSS
};

typedef struct {
    // Per engine, by index in engines[] array (not the same as color)
    int movetime[2];
    int time[2], increment[2];
    int movestogo[2];
    unsigned nodes[2];
    int depth[2];

    int resignCount, resignScore;
    int drawCount, drawScore;
    bool chess960;
} GameOptions;

typedef struct {
    str_t names[NB_COLOR];  // names of players, by color
    Position *pos;  // list of positions (including moves) since game start
    int ply, maxPly;
    int result;
    GameOptions go;

} Game;

Game game_new(const char *fen, const GameOptions *go);
void game_delete(Game *g);

void game_play(Game *g, const Engine engines[2], bool reverse);
str_t game_decode_result(const Game *g, str_t *reason);
str_t game_pgn(const Game *g);
