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
    str_t names[NB_COLOR];  // names of white and black players
    Position *pos;  // list of positions (including moves) since game start
    int ply, maxPly;
    bool chess960;
    int result;
} Game;

Game game_new(bool chess960, const char *fen);
void game_delete(Game *g);

void game_play(Game *g, const Engine *first, const Engine *second);
str_t game_pgn(const Game *g);
