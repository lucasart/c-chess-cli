#pragma once
#include "position.h"

// Max number of pseudo-legal moves allowed
enum {MAX_MOVES = 192};

// Generate pseudo-legal moves (ie. legal modulo self-check)
move_t *gen_pawn_moves(const Position *pos, move_t *mList, bitboard_t filter, bool subPromotions);
move_t *gen_piece_moves(const Position *pos, move_t *mList, bitboard_t filter, bool kingMoves);
move_t *gen_castling_moves(const Position *pos, move_t *mList);
move_t *gen_check_escapes(const Position *pos, move_t *mList, bool subPromotions);

// Verify legality of pseudo-legal moves generates by the above
bool gen_is_legal(const Position *pos, bitboard_t pins, move_t m);

// Count leaves of the full tree (ie. generate all legal moves at each node, no pruning)
uint64_t gen_perft(const Position *pos, int depth, int ply, bool chess960);
