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
#include "gen.h"
#include "position.h"
#include "util.h"

// xorshift64star by Sebastiano Vigna: http://vigna.di.unimi.it/ftp/papers/xorshift.pdf
static uint64_t mix(uint64_t b)
{
    b ^= b >> 12;
    b ^= b << 25;
    b ^= b >> 27;
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

#define TEST(val) do { \
    if (!(val)) { \
        printf("FAIL %s: %d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_bitboard(void)
{
    uint64_t hs, hv, rs;  // hash seed, hash value, and random seed

    // Validate: opposite(), push_inc()
    TEST(opposite(WHITE) == BLACK && opposite(BLACK) == WHITE);
    TEST(push_inc(WHITE) == 8 && push_inc(BLACK) == -8);

    // Validate: square_from()
    hs = hv = 0;

    for (int r = 0; r < NB_RANK; r++)
        for (int f = 0; f < NB_FILE; f++)
            hv ^= hash_block(&hs, (uint64_t)square_from(r, f));

    TEST(hv == 0xdaed883de606a87a);

    // Validate: rank_of(), file_of()
    hs = hv = 0;

    for (int s = 0; s < NB_SQUARE; s++) {
        hv ^= hash_block(&hs, (uint64_t)rank_of(s));
        hv ^= hash_block(&hs, (uint64_t)file_of(s));
    }

    TEST(hv == 0x3f259cdbf7e0e32d);

    // Validate: reltive_rank()
    hs = hv = 0;

    for (int c = 0; c < NB_COLOR; c++)
        for (int r = 0; r < NB_RANK; r++)
            hv ^= hash_block(&hs, (uint64_t)relative_rank(c, r));

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
                hv ^= hash_block(&hs, (uint64_t)m);
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
    hs = hv = rs = 0;

    for (int i = 0; i < 2000; i++) {
        const int s = prng(&rs) % NB_SQUARE;
        const bitboard_t occ = prng(&rs) & prng(&rs);
        hv ^= hash_block(&hs, bb_bishop_attacks(s, occ));
        hv ^= hash_block(&hs, bb_rook_attacks(s, occ));
    }

    TEST(hv == 0x56720d3c08204cb1);

    // Validate: bb_lsb(), bb_msb(), bb_pop_lsb(), bb_several(), bb_count()
    hs = hv = rs = 0;

    for (int i = 0; i < 1000; i++) {
        bitboard_t b = prng(&rs) & prng(&rs);

        hv ^= hash_block(&hs, bb_several(b & prng(&rs)));  // need very sparse bitboard to test
        hv ^= hash_block(&hs, (uint64_t)bb_count(b));

        if (b) {
            hv ^= hash_block(&hs, (uint64_t)bb_lsb(b));
            hv ^= hash_block(&hs, (uint64_t)bb_msb(b));
        }

        while (b)
            hv ^= hash_block(&hs, (uint64_t)bb_pop_lsb(&b));
    }

    TEST(hv == 0xffada1c1b6fe03c);
}

static void test_position(void)
{
    // Validate: pos_set(), pos_get()
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
        "8/5R2/2k5/b1p2p2/2B5/p2K2P1/7P/8 w - - 10",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w",
        "r1k1r2q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K1R2Q w KQkq - 13 5",
        "r1k2r1q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K2R1Q w KQkq - 99 1000",
        "8/8/8/4B2b/6nN/8/5P2/2R1K2k w Q - 0 3",
        "2r5/8/8/8/8/8/6PP/k2KR3 w K - 1 0",
        "4r3/3k4/8/8/8/8/6PP/qR1K1R2 w KQ -",
        NULL
    };

    uint64_t hs = 0, hv = 0;

    for (size_t i = 0; fens[i]; i++) {
        Position pos;
        pos_set(&pos, fens[i]);

        hv ^= hash_blocks(&pos, sizeof(pos), &hs);

        scope(str_del) str_t fen = pos_get(&pos);
        TEST(strncmp(fen.buf, fens[i], strlen(fens[i])) == 0);
    }

    TEST(hv == 0x765fb9f62ca1e277);

    // Validate: pos_lan_to_move(), pos_move(), pos_move_to_san(), and exercise string code
    Position pos[2];
    pos_set(&pos[0], fens[1]);
    const char *moves = "e1g1 e8c8 a2a3 c7c6 g2g4 f6g4 c3a4 f7f5 a3b4 f5e4 d5c6 c8b8 g1h1 g7h6 "
        "d2e6 h8h6 f3e4 d8f8 e4g6";

    const char *tail = moves;
    scope(str_del) str_t token = str_new(), sanMoves = str_new();
    int ply = 0;
    hs = hv = 0;

    while ((tail = str_tok(tail, &token, " "))) {
        move_t m = pos_lan_to_move(&pos[ply % 2], token.buf, false);
        pos_move(&pos[(ply + 1) % 2], &pos[ply % 2], m);
        hv ^= hash_blocks(&pos[(ply + 1) % 2], sizeof(Position), &hs);

        scope(str_del) str_t san = pos_move_to_san(&pos[ply % 2], m);
        str_cat_fmt(&sanMoves, "%S ", &san);

        ply++;
    }

    TEST(hv = 0x32be0aae25ef25b2);
    TEST(!strcmp(sanMoves.buf, "O-O O-O-O a3 c6 g4 Nxg4 Na4 f5 axb4 fxe4 dxc6 Kb8 Kh1 Bh6 Bxe6 Rh6 "
        "Qxe4 Rf8 Qxg6 "));
}

static size_t test_gen_leaves(const Position *pos, int depth, int ply, bool chess960)
{
    if (depth <= 0) {
        assert(depth == 0);
        return 1;
    }

    move_t mList[MAX_MOVES];

    move_t *end = gen_all_moves(pos, mList);
    size_t result = 0;

    // Bulk counting
    if (depth == 1 && ply > 0)
        return (size_t)(end - mList);

    for (move_t *m = mList; m != end; m++) {
        Position after;
        pos_move(&after, pos, *m);
        const size_t subTree = test_gen_leaves(&after, depth - 1, ply + 1, chess960);
        result += subTree;

        /*if (!ply) {
            scope(str_del) str_t lan = pos_move_to_lan(pos, *m, chess960);
            printf("%s\t%" PRIu64 "\n", lan.buf, subTree);
        }*/
    }

    return result;
}

void test_gen(void)
{
    typedef struct {
        const char fen[128];
        size_t leaves;
        int depth;
        char pad[4];
    } Test;

    Test tests[] = {
        // Normal chess: https://www.chessprogramming.org/Perft_Results
        (Test){"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0", 4865609, 5, {0}},
        (Test){"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0", 4085603, 4, {0}},
        (Test){"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -  0", 674624, 5, {0}},
        (Test){"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0", 422333, 4, {0}},
        (Test){"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1", 2103487, 4, {0}},
        (Test){"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0", 3894594, 4, {0}},

        // Chess960: http://talkchess.com/forum3/viewtopic.php?f=7&t=55274#p608193
        (Test){"r1k1r2q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K1R2Q w KQkq - 0", 7096972, 5, {0}},
        (Test){"r1k2r1q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K2R1Q w KQkq - 0", 541480, 4, {0}},
        (Test){"8/8/8/4B2b/6nN/8/5P2/2R1K2k w Q - 0", 3223406, 5, {0}},
        (Test){"2r5/8/8/8/8/8/6PP/k2KR3 w K - 0", 985298, 5, {0}},
        (Test){"4r3/3k4/8/8/8/8/6PP/qR1K1R2 w KQ - 0", 8992652, 5, {0}}
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(Test); i++) {
        Position pos;
        pos_set(&pos, tests[i].fen);

        const size_t leaves = test_gen_leaves(&pos, tests[i].depth, 0, true);

        /*
        pos_print(&pos);

        if (leaves != tests[i].leaves)
            printf("FAILED: fen '%s', depth %i, expected %" PRIu64 ", found %" PRIu64 "\n",
                tests[i].fen, tests[i].depth, tests[i].leaves, leaves);
        */

        TEST(leaves == tests[i].leaves);
    }
}

int main(void)
{
    test_bitboard();
    puts("bitboard ok");

    test_position();
    puts("position ok");

    test_gen();
    puts("gen ok");

    return 0;
}
