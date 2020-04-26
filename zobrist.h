#pragma once
#include "position.h"

enum {MAX_GAME_PLY = 2048};

typedef struct {
    uint64_t keys[MAX_GAME_PLY];
    int idx;
} ZobristStack;

extern uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
extern uint64_t ZobristCastling[NB_SQUARE];
extern uint64_t ZobristEnPassant[NB_SQUARE + 1];
extern uint64_t ZobristTurn;

uint64_t zobrist_castling(bitboard_t castleRooks);

void zobrist_clear(ZobristStack *st);
void zobrist_push(ZobristStack *st, uint64_t key);
void zobrist_pop(ZobristStack *st);
uint64_t zobrist_back(const ZobristStack *st);
uint64_t zobrist_move_key(const ZobristStack *st, int back);
bool zobrist_repetition(const ZobristStack *st, const Position *pos);
