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
#include "vec.h"
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
    bitboard_t byColor[NB_COLOR];
    bitboard_t byPiece[NB_PIECE];
    int16_t score;  // score returned by the engine (in cp)
    int8_t result;  // game result from turn's pov: -1 (loss), 0 (draw), +1 (win)
    uint8_t turn;  // turn of play
    uint8_t epSquare;  // en-passant square (NB_SQUARE if none)
    uint8_t castleRooks[NB_COLOR];  // rooks available for castling; a file bitmask by color
    uint8_t rule50; // ply counter for 50-move rule, from 0 to 99 (100 would be draw or mated)
} Sample;

typedef struct {
    str_t names[NB_COLOR];  // names of players, by color
    Position *pos;  // list of positions (including moves) since game start
    Sample *samples;  // list of samples when generating training data
    int ply, state;
} Game;

Game game_new(const str_t *fen);
void game_delete(Game *g);

int game_play(Game *g, const GameOptions *go, const Engine engines[2], Deadline *deadline,
    bool reverse);
str_t game_decode_state(const Game *g, str_t *reason);
str_t game_pgn(const Game *g);
