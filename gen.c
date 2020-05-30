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
#include <stdio.h>
#include "gen.h"

static move_t *serialize_piece_moves(int from, bitboard_t pins, int king, bitboard_t targets,
    move_t *mList)
{

    if (bb_test(pins, from))
        targets &= Ray[king][from];

    while (targets)
        *mList++ = move_build(from, bb_pop_lsb(&targets), NB_PIECE);

    return mList;
}

static move_t *serialize_pawn_moves(bitboard_t pawns, bitboard_t pins, int king, int shift,
    move_t *mList)
{
    while (pawns) {
        const int from = bb_pop_lsb(&pawns);

        if (!bb_test(pins, from) || bb_test(Ray[king][from], from + shift))
            *mList++ = move_build(from, from + shift, NB_PIECE);
    }

    return mList;
}

static move_t *gen_pawn_moves(const Position *pos, move_t *mList, bitboard_t filter)
{
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);
    const int push = push_inc(us);
    const bitboard_t capturable = pos->byColor[them] & filter;

    // ** Normal moves **
    const bitboard_t nonPromotingPawns = pos_pieces_cp(pos, us, PAWN)
        & ~Rank[relative_rank(us, RANK_7)];

    // Left captures
    bitboard_t b = nonPromotingPawns & ~File[FILE_A] & bb_shift(capturable, -(push + LEFT));
    mList = serialize_pawn_moves(b, pos->pins, king, push + LEFT, mList);

    // Right captures
    b = nonPromotingPawns & ~File[FILE_H] & bb_shift(capturable, -(push + RIGHT));
    mList = serialize_pawn_moves(b, pos->pins, king, push + RIGHT, mList);

    // Single pushes
    b = nonPromotingPawns & bb_shift(~pos_pieces(pos) & filter, -push);
    mList = serialize_pawn_moves(b, pos->pins, king, push, mList);

    // Double pushes
    b = nonPromotingPawns & Rank[relative_rank(us, RANK_2)] & bb_shift(~pos_pieces(pos), -push)
        & bb_shift(~pos_pieces(pos) & filter, -2 * push);
    mList = serialize_pawn_moves(b, pos->pins, king, 2 * push, mList);

    // ** En passant **
    if (pos->epSquare != NB_SQUARE) {
        bitboard_t epCapturingPawns = PawnAttacks[them][pos->epSquare] & nonPromotingPawns;

        while (epCapturingPawns) {
            const int from = bb_pop_lsb(&epCapturingPawns);

            bitboard_t occ = pos_pieces(pos);
            bb_clear(&occ, from);
            bb_set(&occ, pos->epSquare);
            bb_clear(&occ, pos->epSquare + push_inc(them));

            if (!(bb_rook_attacks(king, occ) & pos_pieces_cpp(pos, them, ROOK, QUEEN))
                    && !(bb_bishop_attacks(king, occ) & pos_pieces_cpp(pos, them, BISHOP, QUEEN)))
                *mList++ = move_build(from, pos->epSquare, NB_PIECE);
        }
    }

    // ** Promotions **
    bitboard_t promotingPawns = pos_pieces_cp(pos, us, PAWN) & Rank[relative_rank(us, RANK_7)];

    while (promotingPawns) {
        const int from = bb_pop_lsb(&promotingPawns);

        // Calculate to squares: captures and single pushes
        bitboard_t targets = PawnAttacks[us][from] & capturable;

        if (bb_test(filter & ~pos_pieces(pos), from + push))
            bb_set(&targets, from + push);

        // Generate promotions
        while (targets) {
            const int to = bb_pop_lsb(&targets);

            if (!bb_test(pos->pins, from) || bb_test(Ray[king][from], to))
                for (int prom = QUEEN; prom >= KNIGHT; --prom)
                    *mList++ = move_build(from, to, prom);
        }
    }

    return mList;
}

static move_t *gen_piece_moves(const Position *pos, move_t *mList, bitboard_t filter, bool kingMoves)
{
    const int us = pos->turn;
    const int king = pos_king_square(pos, us);

    // King moves
    if (kingMoves) {
        const int from = pos_king_square(pos, us);
        mList = serialize_piece_moves(from, pos->pins, king, KingAttacks[from] & filter
            & ~pos->attacked, mList);
    }

    // Knight moves
    bitboard_t knights = pos_pieces_cp(pos, us, KNIGHT);

    while (knights) {
        const int from = bb_pop_lsb(&knights);
        mList = serialize_piece_moves(from, pos->pins, king, KnightAttacks[from] & filter, mList);
    }

    // Rook moves
    bitboard_t rookMovers = pos_pieces_cpp(pos, us, ROOK, QUEEN);

    while (rookMovers) {
        const int from = bb_pop_lsb(&rookMovers);
        mList = serialize_piece_moves(from, pos->pins, king,
            bb_rook_attacks(from, pos_pieces(pos)) & filter, mList);
    }

    // Bishop moves
    bitboard_t bishopMovers = pos_pieces_cpp(pos, us, BISHOP, QUEEN);

    while (bishopMovers) {
        const int from = bb_pop_lsb(&bishopMovers);
        mList = serialize_piece_moves(from, pos->pins, king,
            bb_bishop_attacks(from, pos_pieces(pos)) & filter, mList);
    }

    return mList;
}

static move_t *gen_castling_moves(const Position *pos, move_t *mList)
{
    assert(!pos->checkers);
    const int king = pos_king_square(pos, pos->turn);

    bitboard_t rooks = pos->castleRooks & pos->byColor[pos->turn];

    while (rooks) {
        const int rook = bb_pop_lsb(&rooks);
        const int kto = square_from(rank_of(rook), rook > king ? FILE_G : FILE_C);
        const int rto = square_from(rank_of(rook), rook > king ? FILE_F : FILE_D);

        if (bb_count((Segment[king][kto] | Segment[rook][rto]) & pos_pieces(pos)) == 2
                && !(pos->attacked & Segment[king][kto]) && !bb_test(pos->pins, rook))
            *mList++ = move_build(king, rook, NB_PIECE);
    }

    return mList;
}

static move_t *gen_check_escapes(const Position *pos, move_t *mList)
{
    assert(pos->checkers);
    bitboard_t ours = pos->byColor[pos->turn];
    const int king = pos_king_square(pos, pos->turn);

    // King moves
    mList = serialize_piece_moves(king, pos->pins, king, KingAttacks[king] & ~ours & ~pos->attacked,
        mList);

    if (!bb_several(pos->checkers)) {
        // Blocking moves (single checker)
        const int checkerSquare = bb_lsb(pos->checkers);
        const int checkerPiece = pos_piece_on(pos, checkerSquare);

        // sliding check: cover the checking segment, or capture the slider
        bitboard_t targets = BISHOP <= checkerPiece && checkerPiece <= QUEEN
              ? Segment[king][checkerSquare]
              : pos->checkers;

        mList = gen_piece_moves(pos, mList, targets & ~ours, false);

        // pawn check: if epsq is available, then the check must result from a pawn double
        // push, and we also need to consider capturing it en-passant to solve the check.
        if (checkerPiece == PAWN && pos->epSquare < NB_SQUARE)
            bb_set(&targets, pos->epSquare);

        mList = gen_pawn_moves(pos, mList, targets);
    }

    return mList;
}

static size_t gen_leaves(const Position *pos, int depth, int ply, bool chess960)
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
        const size_t subTree = gen_leaves(&after, depth - 1, ply + 1, chess960);
        result += subTree;

        if (!ply) {
            scope(str_del) str_t lan = pos_move_to_lan(pos, *m, chess960);
            printf("%s\t%" PRIu64 "\n", lan.buf, subTree);
        }
    }

    return result;
}

move_t *gen_all_moves(const Position *pos, move_t *mList)
{
    if (pos->checkers)
        return gen_check_escapes(pos, mList);
    else {
        move_t *m = mList;
        m = gen_pawn_moves(pos, m, ~pos->byColor[pos->turn]);
        m = gen_piece_moves(pos, m, ~pos->byColor[pos->turn], true);
        m = gen_castling_moves(pos, m);
        return m;
    }
}

void gen_run_test()
{
    typedef struct {
        const char fen[128];
        int depth;
        size_t leaves;
    } Test;

    Test tests[] = {
        // Normal chess: https://www.chessprogramming.org/Perft_Results
        (Test){"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0", 5, 4865609},
        (Test){"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0", 4, 4085603},
        (Test){"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -  0", 5, 674624},
        (Test){"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0", 4, 422333},
        (Test){"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1", 4, 2103487},
        (Test){"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0", 4, 3894594},

        // Chess960: http://talkchess.com/forum3/viewtopic.php?f=7&t=55274#p608193
        (Test){"r1k1r2q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K1R2Q w KQkq - 0", 5, 7096972},
        (Test){"r1k2r1q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K2R1Q w KQkq - 0", 4, 541480},
        (Test){"8/8/8/4B2b/6nN/8/5P2/2R1K2k w Q - 0", 5, 3223406},
        (Test){"2r5/8/8/8/8/8/6PP/k2KR3 w K - 0", 5, 985298},
        (Test){"4r3/3k4/8/8/8/8/6PP/qR1K1R2 w KQ - 0", 5, 8992652}
    };

    Position pos;

    for (size_t i = 0; i < sizeof(tests) / sizeof(Test); i++) {
        pos_set(&pos, tests[i].fen);
        pos_print(&pos);
        printf("depth %i:\n", tests[i].depth);

        const size_t leaves = gen_leaves(&pos, tests[i].depth, 0, true);

        if (leaves != tests[i].leaves) {
            printf("FAILED: fen '%s', depth %i, expected %" PRIu64 ", found %" PRIu64 "\n",
                tests[i].fen, tests[i].depth, tests[i].leaves, leaves);
            break;
        } else
            puts("OK");
    }
}
