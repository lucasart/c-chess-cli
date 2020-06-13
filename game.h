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
#include "workers.h"

enum {
    STATE_NONE,

    // All possible ways to lose
    STATE_CHECKMATE,  // lost by being checkmated
    STATE_TIME_LOSS,  // lost on time
    STATE_ILLEGAL_MOVE,  // lost by playing an illegal move
    STATE_RESIGN,  // resigned on behalf of the engine

    STATE_SEPARATOR,  // invalid result, just a market to separate losses from draws

    // All possible ways to draw
    STATE_STALEMATE,  // draw by stalemate
    STATE_THREEFOLD,  // draw by 3 position repetition
    STATE_FIFTY_MOVES,  // draw by 50 moves rule
    STATE_INSUFFICIENT_MATERIAL,  // draw due to insufficient material to deliver checkmate
    STATE_DRAW_ADJUDICATION  // draw by adjudication
};

enum {
    RESULT_WIN,
    RESULT_LOSS,
    RESULT_DRAW
};

typedef struct {
    // Per engine, by index in engines[] array (not the same as color)
    int64_t movetime[2], time[2], increment[2];
    uint64_t nodes[2];
    int movestogo[2];
    int depth[2];
    int resignCount, resignScore;
    int drawCount, drawScore;
    bool chess960;
    char pad[7];
} GameOptions;

typedef struct {
    str_t names[NB_COLOR];  // names of players, by color
    Position *pos;  // list of positions (including moves) since game start
    GameOptions go;
    int ply, maxPly, state;
    char pad[4];
} Game;

Game game_new(const str_t *fen, const GameOptions *go);
void game_delete(Game *g);

int game_play(Game *g, const Engine engines[2], Deadline *deadline, bool reverse);
str_t game_decode_state(const Game *g, str_t *reason);
str_t game_pgn(const Game *g);
