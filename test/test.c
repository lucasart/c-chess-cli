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
#include "gen.h"
#include "util.h"

static uint64_t hash_mix(uint64_t block)
{
    block ^= block >> 23;
    block *= 0x2127599bf4325c37ULL;
    return block ^= block >> 47;
}

void hash_block(uint64_t block, uint64_t *hash)
{
    *hash ^= hash_mix(block);
    *hash *= 0x880355f21e6d1965ULL;
}

// Based on FastHash64, but I had to change the functionality to make it capable of incremental
// updates. Assumes length is divisible by 8, and buffer is 8-byte aligned.
void hash_blocks(const void *buffer, size_t length, uint64_t *hash)
{
    assert((uintptr_t)buffer % 8 == 0 && length % 8 == 0);
    const uint64_t *blocks = (const uint64_t *)buffer;

    for (size_t i = 0; i < length / 8; i++)
        hash_block(*blocks++, hash);
}

#define TEST(val) do { \
    if (!(val)) { \
        printf("FAIL %s: %d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_bitboard(void)
{
    // Validate: opposite(), push_inc()
    TEST(opposite(WHITE) == BLACK && opposite(BLACK) == WHITE);
    TEST(push_inc(WHITE) == 8 && push_inc(BLACK) == -8);

    // Validate: square_from()
    uint64_t hash = 0;

    for (int r = 0; r < NB_RANK; r++)
        for (int f = 0; f < NB_FILE; f++)
            hash_block((uint64_t)square_from(r, f), &hash);

    TEST(hash == 0x772e417be2bb89c1);

    // Validate: rank_of(), file_of()
    hash = 0;

    for (int s = 0; s < NB_SQUARE; s++) {
        hash_block((uint64_t)rank_of(s), &hash);
        hash_block((uint64_t)file_of(s), &hash);
    }

    TEST(hash == 0xc28324f57afc7e70);

    // Validate: reltive_rank()
    hash = 0;

    for (int c = 0; c < NB_COLOR; c++)
        for (int r = 0; r < NB_RANK; r++)
            hash_block((uint64_t)relative_rank(c, r), &hash);

    TEST(hash == 0x816be3abc20c91e0);

    // Validate: move_build(), move_from(), move_to(), move_prom()
    hash = 0;

    for (int from = 0; from < NB_SQUARE; from++)
        for (int to = 0; to < NB_SQUARE; to++)
            for (int prom = 0; prom <= NB_PIECE; prom++) {
                if (prom == KING || prom == PAWN)
                    continue;

                const move_t m = move_build(from, to, prom);
                TEST(move_from(m) == from && move_to(m) == to && move_prom(m) == prom);
                hash_block((uint64_t)m, &hash);
            }

    TEST(hash == 0xc80d6ebde95a5cd0);

    // Validate: Rank[], File[], Ray[], Segment[], PawnAttacks[], KnightAttacks[], KingAttacks[]
    hash = 0;

    hash_blocks(Rank, sizeof Rank, &hash);
    hash_blocks(File, sizeof File, &hash);
    hash_blocks(Ray, sizeof Ray, &hash);
    hash_blocks(Segment, sizeof Segment, &hash);
    hash_blocks(PawnAttacks, sizeof PawnAttacks, &hash);
    hash_blocks(KnightAttacks, sizeof KnightAttacks, &hash);
    hash_blocks(KingAttacks, sizeof KingAttacks, &hash);

    TEST(hash == 0x86d8f993949b5c69);

    // Validate: bb_bishop_attacks() and bb_rook_attacks()
    uint64_t seed = hash = 0;

    for (int i = 0; i < 2000; i++) {
        const int s = prng(&seed) % NB_SQUARE;
        const bitboard_t occ = prng(&seed) & prng(&seed);
        hash_block(bb_bishop_attacks(s, occ), &hash);
        hash_block(bb_rook_attacks(s, occ), &hash);
    }

    TEST(hash == 0x3cf6b20ccc349d24);

    // Validate: bb_lsb(), bb_msb(), bb_pop_lsb(), bb_several(), bb_count()
    seed = hash = 0;

    for (int i = 0; i < 1000; i++) {
        bitboard_t b = prng(&seed) & prng(&seed);

        hash_block(bb_several(b & prng(&seed)), &hash);  // need very sparse bitboard to test
        hash_block((uint64_t)bb_count(b), &hash);

        if (b) {
            hash_block((uint64_t)bb_lsb(b), &hash);
            hash_block((uint64_t)bb_msb(b), &hash);
        }

        while (b)
            hash_block((uint64_t)bb_pop_lsb(&b), &hash);
    }

    TEST(hash == 0x1958414c63e413ec);
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

    uint64_t hash = 0;

    for (size_t i = 0; fens[i]; i++) {
        Position pos;
        const str_t inFen = str_ref(fens[i]);
        pos_set(&pos, inFen, false);

        hash_blocks(&pos, sizeof(pos), &hash);

        scope(str_del) str_t outFen = pos_get(&pos);
        TEST(strncmp(inFen.buf, outFen.buf, inFen.len) == 0);
    }

    TEST(hash == 0x62f5f994d63574ca);

    // Validate: pos_lan_to_move(), pos_move_to_lan(), pos_move(), pos_move_to_san(), and exercise
    // string code
    Position pos[2];
    const str_t fen = str_ref(fens[1]);
    pos_set(&pos[0], fen, false);
    const char *moves = "e1g1 e8c8 a2a3 c7c6 g2g4 f6g4 c3a4 f7f5 a3b4 f5e4 d5c6 c8b8 g1h1 g7h6 "
        "d2h6 h8h6 f3e4 d8f8 e4g6 ";

    const char *tail = moves;
    scope(str_del) str_t token = {0}, sanMoves = {0}, lanMoves= {0};
    int ply = 0;
    hash = 0;

    while ((tail = str_tok(tail, &token, " "))) {
        move_t m = pos_lan_to_move(&pos[ply % 2], token);
        pos_move(&pos[(ply + 1) % 2], &pos[ply % 2], m);
        hash_blocks(&pos[(ply + 1) % 2], sizeof(Position), &hash);

        str_push(pos_move_to_lan(&pos[ply % 2], m, &lanMoves), ' ');
        str_push(pos_move_to_san(&pos[ply % 2], m, &sanMoves), ' ');
        ply++;
    }

    TEST(hash == 0x4ce2254fc1869ebe);
    TEST(!strcmp(lanMoves.buf, moves));
    TEST(!strcmp(sanMoves.buf, "O-O O-O-O a3 c6 g4 Nxg4 Na4 f5 axb4 fxe4 dxc6 Kb8 Kh1 Bh6 Bxh6 "
        "Rxh6 Qxe4 Rf8 Qxg6 "));
}

static size_t test_gen_leaves(const Position *pos, int depth, int ply)
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
        const size_t subTree = test_gen_leaves(&after, depth - 1, ply + 1);
        result += subTree;

        /*if (!ply) {
            scope(str_del) str_t lan = pos_move_to_lan(pos, *m);
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
        (Test){"r1k2r1q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K2R1Q w AFaf - 0", 541480, 4, {0}},
        (Test){"8/8/8/4B2b/6nN/8/5P2/2R1K2k w Q - 0", 3223406, 5, {0}},
        (Test){"2r5/8/8/8/8/8/6PP/k2KR3 w K - 0", 985298, 5, {0}},
        (Test){"4r3/3k4/8/8/8/8/6PP/qR1K1R2 w BF - 0", 8992652, 5, {0}}
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(Test); i++) {
        Position pos;
        const str_t fen = str_ref(tests[i].fen);
        pos_set(&pos, fen, true);

        const size_t leaves = test_gen_leaves(&pos, tests[i].depth, 0);

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
