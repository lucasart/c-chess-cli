#pragma once
#include "position.h"

enum {MAX_MOVES = 192};

// Redundant information deduced from a position, needed to generate legal moves
typedef struct {
    uint64_t attacked;  // squares attacked by enemy
    uint64_t checkers;  // if in check, enemy piece(s) giving check(s), otherwise empty
    uint64_t pins;  // pinned pieces for the side to move
} GenInfo;

move_t *gen_all_moves(const Position *pos, GenInfo *gi, move_t *mList);

void gen_run_test();
