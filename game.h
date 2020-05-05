#pragma once
#include "position.h"
#include "engine.h"

enum {MAX_GAME_PLY = 2048};

enum {
    RESULT_NONE,
    RESULT_CHECKMATE,  // lost by being checkmated
    RESULT_STALEMATE,  // draw by stalemate
    RESULT_THREEFOLD,  // draw by 3 position repetition
    RESULT_FIFTY_MOVES,  // draw by 50 moves rule
    RESULT_INSUFFICIENT_MATERIAL,  // draw due to insufficient material to deliver checkmate
    RESULT_ILLEGAL_MOVE,  // lost by playing an illegal move
    RESULT_MAX_PLY  // not supposed to happen (count as draw but notify user)
};

typedef struct {
    Position pos[MAX_GAME_PLY];  // list of positions (including moves) since game start
    str_t names[NB_COLOR];  // names of white and black players
    int ply;
    bool chess960;
    int result;
} Game;

void game_create(Game *g, bool chess960, const char *fen);
void game_destroy(Game *g);

void game_play(Game *g, const Engine *first, const Engine *second);
void game_print(const Game *g, FILE *out);
