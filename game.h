#pragma once
#include "position.h"
#include "engine.h"

enum {
    MAX_GAME_PLY = 2048,
    MAX_POSITION_CHAR = 9 + MAX_FEN_CHAR + 7 + 100 * MAX_MOVE_CHAR + 1
};

typedef struct {
    Engine e[2];
    Position pos[MAX_GAME_PLY];  // list of positions since game start
    int ply;
    bool chess960;
} Game;

void play_game(Game *g);
