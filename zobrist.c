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
#include "gen.h"
#include "util.h"
#include "zobrist.h"

uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
uint64_t ZobristCastling[NB_SQUARE];
uint64_t ZobristEnPassant[NB_SQUARE + 1];
uint64_t ZobristTurn;

static __attribute__((constructor)) void zobrist_init()
{
    uint64_t state = 0;

    for (int color = WHITE; color <= BLACK; color++)
        for (int piece = KNIGHT; piece < NB_PIECE; piece++)
            for (int square = A1; square <= H8; square++)
                ZobristKey[color][piece][square] = prng(&state);

    for (int square = A1; square <= H8; square++) {
        ZobristCastling[square] = prng(&state);
        ZobristEnPassant[square] = prng(&state);
    }

    ZobristEnPassant[NB_SQUARE] = prng(&state);
    ZobristTurn = prng(&state);
}

uint64_t zobrist_castling(bitboard_t castleRooks)
{
    bitboard_t k = 0;

    while (castleRooks)
        k ^= ZobristCastling[bb_pop_lsb(&castleRooks)];

    return k;
}

void zobrist_clear(ZobristStack *st)
{
    st->idx = 0;
}

void zobrist_push(ZobristStack *st, uint64_t key)
{
    assert(0 <= st->idx && st->idx < MAX_GAME_PLY);
    st->keys[st->idx++] = key;
}

void zobrist_pop(ZobristStack *st)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    st->idx--;
}

uint64_t zobrist_back(const ZobristStack *st)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    return st->keys[st->idx - 1];
}

uint64_t zobrist_move_key(const ZobristStack *st, int back)
{
    assert(0 < st->idx && st->idx <= MAX_GAME_PLY);
    return st->idx - 1 - back > 0
        ? st->keys[st->idx - 1 - back] ^ st->keys[st->idx - 2 - back]
        : 0;
}

bool zobrist_repetition(const ZobristStack *st, const Position *pos)
{
    // 50 move rule
    if (pos->rule50 >= 100) {
        // If're not mated here, it's draw
        move_t mList[MAX_MOVES];
        return !pos->checkers || gen_check_escapes(pos, mList, false) != mList;
    }

    // TODO: use 3 repetition past root position
    for (int i = 4; i <= pos->rule50 && i < st->idx; i += 2)
        if (st->keys[st->idx - 1 - i] == st->keys[st->idx - 1])
            return true;

    return false;
}
