#pragma once
#include "position.h"

enum {MAX_MOVES = 192};

move_t *gen_all_moves(const Position *pos, move_t *mList);

void gen_run_test();
