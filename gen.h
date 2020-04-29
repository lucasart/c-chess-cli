#pragma once
#include "position.h"

// Max number of pseudo-legal moves allowed
enum {MAX_MOVES = 192};

// Generate pseudo-legal moves (ie. legal modulo self-check)
move_t *gen_all_moves(const Position *pos, move_t *mList);

// Verify legality of pseudo-legal moves generates by the above
bool gen_is_legal(const Position *pos, bitboard_t pins, move_t m);

// Run a set of test positions to verify the move generators and board manipulation code
void gen_run_test();
