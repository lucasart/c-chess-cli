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

// Stand alone program, running unit tests for c-chess-cli

#include <stdio.h>
#include "bitboard.h"
#include "util.h"

// xorshift64star by Sebastiano Vigna: http://vigna.di.unimi.it/ftp/papers/xorshift.pdf
uint64_t mix(uint64_t b)
{
    b ^= b >> 12, b ^= b << 25, b ^= b >> 27;
    return b * 2685821657736338717LL;
}

static uint64_t hash_block(uint64_t *seed, uint64_t block)
{
    return prng(seed) + mix(block);
}

static uint64_t hash_blocks(const void *buffer, size_t length, uint64_t *seed)
{
    assert((uintptr_t)buffer % 8 == 0 && length % 8 == 0);
    const uint64_t *blocks = (const uint64_t *)buffer;
    uint64_t result = 0;

    for (size_t i = 0; i < length / 8; i++)
        result ^= hash_block(seed, *blocks++);

    return result;
}

#define TEST(val) ({ \
    if (!(val)) { \
        printf("FAIL %s: %d\n", __FILE__, __LINE__); \
        return; \
    } \
})

void test_bitboard(void)
{
    uint64_t hs, hv, rs;  // hash seed, hash value, and random seed

    // Validate: opposite(), push_inc()
    TEST(opposite(WHITE) == BLACK && opposite(BLACK) == WHITE);
    TEST(push_inc(WHITE) == 8 && push_inc(BLACK) == -8);

    // Validate: square_from()
    hs = hv = 0;
    for (int r = 0; r < NB_RANK; r++)
        for (int f = 0; f < NB_FILE; f++)
            hv ^= hash_block(&hs, square_from(r, f));
    TEST(hv == 0xdaed883de606a87a);

    // Validate: rank_of(), file_of()
    hs = hv = 0;
    for (int s = 0; s < NB_SQUARE; s++) {
        hv ^= hash_block(&hs, rank_of(s));
        hv ^= hash_block(&hs, file_of(s));
    }
    TEST(hv == 0x3f259cdbf7e0e32d);

    // Validate: reltive_rank()
    hs = hv = 0;
    for (int c = 0; c < NB_COLOR; c++) 
        for (int r = 0; r < NB_RANK; r++)
            hv ^= hash_block(&hs, relative_rank(c, r));
    TEST(hv = 0x3069d65765d6619c);

    // Validate: move_build(), move_from(), move_to(), move_prom()
    hs = hv = 0;
    for (int from = 0; from < NB_SQUARE; from++)
        for (int to = 0; to < NB_SQUARE; to++)
            for (int prom = 0; prom <= NB_PIECE; prom++) {
                if (prom == KING || prom == PAWN)
                    continue;

                const move_t m = move_build(from, to, prom);
                TEST(move_from(m) == from && move_to(m) == to && move_prom(m) == prom);
                hv ^= hash_block(&hs, m);
            }
    TEST(hv == 0xcc9192e4a5b7c4a2);

    // Validate: Rank[], File[], Ray[], Segment[], PawnAttacks[], KnightAttacks[], KingAttacks[]
    hs = hv = 0;
    hv ^= hash_blocks(Rank, sizeof Rank, &hs);
    hv ^= hash_blocks(File, sizeof File, &hs);
    hv ^= hash_blocks(Ray, sizeof Ray, &hs);
    hv ^= hash_blocks(Segment, sizeof Segment, &hs);
    hv ^= hash_blocks(PawnAttacks, sizeof PawnAttacks, &hs);
    hv ^= hash_blocks(KnightAttacks, sizeof KnightAttacks, &hs);
    hv ^= hash_blocks(KingAttacks, sizeof KingAttacks, &hs);
    TEST(hv == 0x8ee00e49243de3f9);

    // Validate: bb_bishop_attacks() and bb_rook_attacks()
    hs = hv = 0, rs = 0;
    for (int i = 0; i < 2000; i++) {
        const int s = prng(&rs) % NB_SQUARE;
        const bitboard_t occ = prng(&rs) & prng(&rs);
        hv ^= hash_block(&hs, bb_bishop_attacks(s, occ));
        hv ^= hash_block(&hs, bb_rook_attacks(s, occ));
    }
    TEST(hv == 0x56720d3c08204cb1);

    // Validate: bb_lsb(), bb_msb(), bb_pop_lsb(), bb_several(), bb_count()
    hs = hv = 0, rs = 0;
    for (int i = 0; i < 1000; i++) {
        bitboard_t b = prng(&rs) & prng(&rs);

        hv ^= hash_block(&hs, bb_several(b) & prng(&rs));  // need very sparse bitboard to test
        hv ^= hash_block(&hs, bb_count(b));

        if (b) {
            hv ^= hash_block(&hs, bb_lsb(b));
            hv ^= hash_block(&hs, bb_msb(b));
        }

        while (b)
            hv ^= hash_block(&hs, bb_pop_lsb(&b));
    }
    TEST(hv == 0x481381ae119f7f7e);
    //printf("%" PRIx64 "\n", hv);
}

int main(void)
{
    test_bitboard();
    return 0;
}
