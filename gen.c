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
#include "bitboard.h"
#include "gen.h"
#include "position.h"

static move_t *serialize_moves(int from, bitboard_t targets, move_t *mList)
{
    while (targets)
        *mList++ = move_build(from, bb_pop_lsb(&targets), NB_PIECE);

    return mList;
}

static move_t *serialize_pawn_moves(bitboard_t pawns, int shift, move_t *mList)
{
    while (pawns) {
        const int from = bb_pop_lsb(&pawns);
        *mList++ = move_build(from, from + shift, NB_PIECE);
    }

    return mList;
}

static move_t *gen_all_moves(const Position *pos, move_t *mList)
{
    if (pos->checkers)
        return gen_check_escapes(pos, mList, true);
    else {
        move_t *m = mList;
        m = gen_pawn_moves(pos, m, ~pos->byColor[pos->turn], true);
        m = gen_piece_moves(pos, m, ~pos->byColor[pos->turn], true);
        m = gen_castling_moves(pos, m);
        return m;
    }
}

move_t *gen_pawn_moves(const Position *pos, move_t *mList, bitboard_t filter, bool subPromotions)
{
    const int us = pos->turn, them = opposite(us);
    const int push = push_inc(us);
    const bitboard_t capturable = (pos->byColor[them] | pos_ep_square_bb(pos)) & filter;

    // ** Non promotions **
    const bitboard_t nonPromotingPawns = pos_pieces_cp(pos, us, PAWN)
        & ~Rank[relative_rank(us, RANK_7)];

    // Left captures
    bitboard_t b = nonPromotingPawns & ~File[FILE_A] & bb_shift(capturable, -(push + LEFT));
    mList = serialize_pawn_moves(b, push + LEFT, mList);

    // Right captures
    b = nonPromotingPawns & ~File[FILE_H] & bb_shift(capturable, -(push + RIGHT));
    mList = serialize_pawn_moves(b, push + RIGHT, mList);

    // Single pushes
    b = nonPromotingPawns & bb_shift(~pos_pieces(pos) & filter, -push);
    mList = serialize_pawn_moves(b, push, mList);

    // Double pushes
    b = nonPromotingPawns & Rank[relative_rank(us, RANK_2)] & bb_shift(~pos_pieces(pos), -push)
        & bb_shift(~pos_pieces(pos) & filter, -2 * push);
    mList = serialize_pawn_moves(b, 2 * push, mList);

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

            if (subPromotions) {
                for (int prom = QUEEN; prom >= KNIGHT; --prom)
                    *mList++ = move_build(from, to, prom);
            } else
                *mList++ = move_build(from, to, QUEEN);
        }
    }

    return mList;
}

move_t *gen_piece_moves(const Position *pos, move_t *mList, bitboard_t filter, bool kingMoves)
{
    const int us = pos->turn;
    int from;

    // King moves
    if (kingMoves) {
        from = pos_king_square(pos, us);
        mList = serialize_moves(from, KingAttacks[from] & filter & ~pos->attacked, mList);
    }

    // Knight moves
    bitboard_t knights = pos_pieces_cp(pos, us, KNIGHT);

    while (knights) {
        from = bb_pop_lsb(&knights);
        mList = serialize_moves(from, KnightAttacks[from] & filter, mList);
    }

    // Rook moves
    bitboard_t rookMovers = pos_pieces_cpp(pos, us, ROOK, QUEEN);

    while (rookMovers) {
        from = bb_pop_lsb(&rookMovers);
        mList = serialize_moves(from, bb_rook_attacks(from, pos_pieces(pos)) & filter, mList);
    }

    // Bishop moves
    bitboard_t bishopMovers = pos_pieces_cpp(pos, us, BISHOP, QUEEN);

    while (bishopMovers) {
        from = bb_pop_lsb(&bishopMovers);
        mList = serialize_moves(from, bb_bishop_attacks(from, pos_pieces(pos)) & filter, mList);
    }

    return mList;
}

move_t *gen_castling_moves(const Position *pos, move_t *mList)
{
    assert(!pos->checkers);
    const int king = pos_king_square(pos, pos->turn);

    bitboard_t rooks = pos->castleRooks & pos->byColor[pos->turn];

    while (rooks) {
        const int rook = bb_pop_lsb(&rooks);
        const int kto = square_from(rank_of(rook), rook > king ? FILE_G : FILE_C);
        const int rto = square_from(rank_of(rook), rook > king ? FILE_F : FILE_D);

        if (bb_count((Segment[king][kto] | Segment[rook][rto]) & pos_pieces(pos)) == 2
                && !(pos->attacked & Segment[king][kto]))
            *mList++ = move_build(king, rook, NB_PIECE);
    }

    return mList;
}

move_t *gen_check_escapes(const Position *pos, move_t *mList, bool subPromotions)
{
    assert(pos->checkers);
    bitboard_t ours = pos->byColor[pos->turn];
    const int king = pos_king_square(pos, pos->turn);

    // King moves
    mList = serialize_moves(king, KingAttacks[king] & ~ours & ~pos->attacked, mList);

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

        mList = gen_pawn_moves(pos, mList, targets, subPromotions);
    }

    return mList;
}

bool gen_is_legal(const Position *pos, bitboard_t pins, move_t m)
{
    const int from = move_from(m), to = move_to(m);
    const int piece = pos_piece_on(pos, from);
    const int king = pos_king_square(pos, pos->turn);

    if (piece == KING) {
        if (bb_test(pos->byColor[pos->turn], to)) {
            // Castling: king can't move through attacked square, and rook can't be pinned
            assert(pos_piece_on(pos, to) == ROOK);
            return !bb_test(pins, to);
        } else
            // Normal king move: do not land on an attacked square (already filtered at generation)
            return true;
    } else {
        // Normal case: illegal if pinned, and moves out of pin-ray
        if (bb_test(pins, from) && !bb_test(Ray[king][from], to))
            return false;

        // En-passant special case: also illegal if self-check through the en-passant captured pawn
        if (to == pos->epSquare && piece == PAWN) {
            const int us = pos->turn, them = opposite(us);
            bitboard_t occ = pos_pieces(pos);
            bb_clear(&occ, from);
            bb_set(&occ, to);
            bb_clear(&occ, to + push_inc(them));
            return !(bb_rook_attacks(king, occ) & pos_pieces_cpp(pos, them, ROOK, QUEEN))
                && !(bb_bishop_attacks(king, occ) & pos_pieces_cpp(pos, them, BISHOP, QUEEN));
        } else
            return true;
    }
}

uint64_t gen_perft(const Position *pos, int depth, int ply, bool chess960)
{
    if (depth <= 0)
        return 1;

    const bitboard_t pins = calc_pins(pos);
    uint64_t result = 0;
    move_t mList[MAX_MOVES], *end = gen_all_moves(pos, mList);

    for (move_t *m = mList; m != end; m++) {
        if (!gen_is_legal(pos, pins, *m))
            continue;

        Position after;
        pos_move(&after, pos, *m);
        const uint64_t subTree = gen_perft(&after, depth - 1, ply + 1, chess960);
        result += subTree;

        if (!ply) {
            char str[6];
            pos_move_to_string(pos, *m, str, chess960);
            printf("%s\t%" PRIu64 "\n", str, subTree);
        }
    }

    return result;
}
