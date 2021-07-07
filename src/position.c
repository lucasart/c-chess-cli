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
#include "position.h"
#include "util.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};
static const char *FileLabel[NB_COLOR] = {"ABCDEFGH", "abcdefgh"};

static uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
static uint64_t ZobristCastling[NB_SQUARE], ZobristEnPassant[NB_SQUARE + 1], ZobristTurn;

static __attribute__((constructor)) void zobrist_init(void) {
    uint64_t seed = 0;

    for (int color = 0; color < NB_COLOR; color++)
        for (int piece = 0; piece < NB_PIECE; piece++)
            for (int square = 0; square < NB_SQUARE; square++)
                ZobristKey[color][piece][square] = prng(&seed);

    for (int square = 0; square < NB_COLOR; square++) {
        ZobristCastling[square] = prng(&seed);
        ZobristEnPassant[square] = prng(&seed);
    }

    ZobristEnPassant[NB_SQUARE] = prng(&seed);
    ZobristTurn = prng(&seed);
}

static uint64_t zobrist_castling(bitboard_t castleRooks) {
    bitboard_t k = 0;

    while (castleRooks)
        k ^= ZobristCastling[bb_pop_lsb(&castleRooks)];

    return k;
}

static void square_to_string(int square, char str[3]) {
    BOUNDS(square, NB_SQUARE + 1);

    if (square == NB_SQUARE)
        *str++ = '-';
    else {
        *str++ = (char)file_of(square) + 'a';
        *str++ = (char)rank_of(square) + '1';
    }

    *str = '\0';
}

static int string_to_square(const char *str) {
    if (str[0] == '-')
        return NB_SQUARE; // none
    else {
        const int rank = str[1] - '1', file = str[0] - 'a';

        if ((unsigned)rank > RANK_8 || (unsigned)file > FILE_H)
            return NB_SQUARE + 1; // syntax error
        else
            return square_from(rank, file);
    }
}

// Remove 'piece' of 'color' on 'square'. Such a piece must be there first.
static void clear_square(Position *pos, int color, int piece, int square) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    BOUNDS(square, NB_SQUARE);

    bb_clear(&pos->byColor[color], square);
    bb_clear(&pos->byPiece[piece], square);
    pos->key ^= ZobristKey[color][piece][square];
}

// Put 'piece' of 'color' on 'square'. Square must be empty first.
static void set_square(Position *pos, int color, int piece, int square) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    BOUNDS(square, NB_SQUARE);

    bb_set(&pos->byColor[color], square);
    bb_set(&pos->byPiece[piece], square);
    pos->key ^= ZobristKey[color][piece][square];
}

static void finish(Position *pos) {
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);

    // ** Calculate pos->pins **

    pos->pins = 0;
    bitboard_t pinners = (pos_pieces_cpp(pos, them, ROOK, QUEEN) & bb_rook_attacks(king, 0)) |
                         (pos_pieces_cpp(pos, them, BISHOP, QUEEN) & bb_bishop_attacks(king, 0));

    while (pinners) {
        const int pinner = bb_pop_lsb(&pinners);
        bitboard_t skewered = Segment[king][pinner] & pos_pieces(pos);
        bb_clear(&skewered, king);
        bb_clear(&skewered, pinner);

        if (!bb_several(skewered) && (skewered & pos->byColor[us]))
            pos->pins |= skewered;
    }

    // ** Calculate pos->attacked **

    // King and Knight attacks
    pos->attacked = KingAttacks[pos_king_square(pos, them)];
    bitboard_t knights = pos_pieces_cp(pos, them, KNIGHT);

    while (knights)
        pos->attacked |= KnightAttacks[bb_pop_lsb(&knights)];

    // Pawn captures
    const bitboard_t pawns = pos_pieces_cp(pos, them, PAWN);
    pos->attacked |= bb_shift(pawns & ~File[FILE_A], push_inc(them) + LEFT);
    pos->attacked |= bb_shift(pawns & ~File[FILE_H], push_inc(them) + RIGHT);

    // Sliders (using modified occupancy to see through a checked king)
    const bitboard_t occ = pos_pieces(pos) ^ pos_pieces_cp(pos, opposite(them), KING);
    bitboard_t rookMovers = pos_pieces_cpp(pos, them, ROOK, QUEEN);

    while (rookMovers)
        pos->attacked |= bb_rook_attacks(bb_pop_lsb(&rookMovers), occ);

    bitboard_t bishopMovers = pos_pieces_cpp(pos, them, BISHOP, QUEEN);

    while (bishopMovers)
        pos->attacked |= bb_bishop_attacks(bb_pop_lsb(&bishopMovers), occ);

    // ** Calculate pos->checkers **

    if (bb_test(pos->attacked, king)) {
        pos->checkers =
            (pos_pieces_cp(pos, them, PAWN) & PawnAttacks[us][king]) |
            (pos_pieces_cp(pos, them, KNIGHT) & KnightAttacks[king]) |
            (pos_pieces_cpp(pos, them, ROOK, QUEEN) & bb_rook_attacks(king, pos_pieces(pos))) |
            (pos_pieces_cpp(pos, them, BISHOP, QUEEN) & bb_bishop_attacks(king, pos_pieces(pos)));

        // We can't be checked by the opponent king
        assert(!(pos_pieces_cp(pos, them, KING) & KingAttacks[king]));

        // Since our king is attacked, we must have at least one checker. Also more than 2 checkers
        // is impossible (even in Chess960).
        assert(pos->checkers && bb_count(pos->checkers) <= 2);
    } else
        pos->checkers = 0;
}

static bool pos_move_is_capture(const Position *pos, move_t m)
// Detect normal captures only (not en passant)
{
    return bb_test(pos->byColor[opposite(pos->turn)], move_to(m));
}

bool pos_set(Position *pos, const char *fen, bool force960)
// Set position from FEN string.
// force960: if true, set pos.chess60=true, else auto-detect.
{
    *pos = (Position){.fullMove = 1};
    scope(str_destroy) str_t token = str_init();

    // Piece placement
    fen = str_tok(fen, &token, " ");
    int rank = RANK_8, file = FILE_A;

    for (const char *c = token.buf; *c; c++) {
        if ('1' <= *c && *c <= '8') {
            file += *c - '0';

            if (file > NB_FILE)
                return false;
        } else if (*c == '/') {
            rank--;
            file = FILE_A;
        } else {
            if (!strchr("nbrqkpNBRQKP", *c))
                return false;

            const bool color = islower((unsigned char)*c);
            set_square(pos, color, (int)(strchr(PieceLabel[color], *c) - PieceLabel[color]),
                       square_from(rank, file++));
        }
    }

    if (rank != RANK_1)
        return false;

    // Turn of play
    fen = str_tok(fen, &token, " ");

    if (token.len != 1)
        return false;

    if (token.buf[0] == 'w')
        pos->turn = WHITE;
    else {
        if (token.buf[0] != 'b')
            return false;

        pos->turn = BLACK;
        pos->key ^= ZobristTurn;
    }

    // Castling rights: optional, default '-'
    if ((fen = str_tok(fen, &token, " "))) {
        if (token.len > 4)
            return false;

        for (const char *c = token.buf; *c; c++) {
            rank = isupper((unsigned char)*c) ? RANK_1 : RANK_8;
            const bitboard_t ourRooks = pos_pieces_cp(pos, rank / RANK_8, ROOK);
            const char uc = (char)toupper(*c);

            if (uc == 'K')
                bb_set(&pos->castleRooks, bb_msb(Rank[rank] & ourRooks));
            else if (uc == 'Q')
                bb_set(&pos->castleRooks, bb_lsb(Rank[rank] & ourRooks));
            else if ('A' <= uc && uc <= 'H')
                bb_set(&pos->castleRooks, square_from(rank, uc - 'A'));
            else if (*c != '-' || pos->castleRooks || c[1] != '\0')
                return false;
        }
    }

    pos->key ^= zobrist_castling(pos->castleRooks);

    // Chess960 auto-detect
    pos->chess960 = force960;
    bitboard_t rooks = pos->castleRooks;

    while (rooks && !pos->chess960) {
        const int rook = bb_pop_lsb(&rooks);
        const int king = pos_king_square(pos, pos_color_on(pos, rook));

        if ((FILE_A < file_of(rook) && file_of(rook) < FILE_H) || file_of(king) != FILE_E)
            pos->chess960 = true;
    }

    // En passant square: optional, default '-'
    if (!(fen = str_tok(fen, &token, " ")))
        str_cpy_c(&token, "-");

    if (token.len > 2)
        return false;

    pos->epSquare = (uint8_t)string_to_square(token.buf);

    if (pos->epSquare > NB_SQUARE)
        return false;

    pos->key ^= ZobristEnPassant[pos->epSquare];

    // Optional: 50 move counter (in plies, starts at 0)
    if ((fen = str_tok(fen, &token, " ")) &&
        (!str_to_uint8(token.buf, &pos->rule50) || pos->rule50 >= 100))
        return false;

    // Optional: full move counter (in moves, starts at 1)
    if ((fen = str_tok(fen, &token, " ")) &&
        (!str_to_uint16(token.buf, &pos->fullMove) || pos->fullMove < 1))
        return false;

    // Verify piece counts
    for (int color = WHITE; color <= BLACK; color++)
        if (bb_count(pos_pieces_cpp(pos, color, KNIGHT, PAWN)) > 10 ||
            bb_count(pos_pieces_cpp(pos, color, BISHOP, PAWN)) > 10 ||
            bb_count(pos_pieces_cpp(pos, color, ROOK, PAWN)) > 10 ||
            bb_count(pos_pieces_cpp(pos, color, QUEEN, PAWN)) > 9 ||
            bb_count(pos_pieces_cp(pos, color, PAWN)) > 8 ||
            bb_count(pos_pieces_cp(pos, color, KING)) != 1 || bb_count(pos->byColor[color]) > 16)
            return false;

    // Verify pawn ranks
    if (pos->byPiece[PAWN] & (Rank[RANK_1] | Rank[RANK_8]))
        return false;

    // Verify castle rooks
    if (pos->castleRooks) {
        if (pos->castleRooks & ~((Rank[RANK_1] & pos_pieces_cp(pos, WHITE, ROOK)) |
                                 (Rank[RANK_8] & pos_pieces_cp(pos, BLACK, ROOK))))
            return false;

        for (int color = WHITE; color <= BLACK; color++) {
            const bitboard_t b = pos->castleRooks & pos->byColor[color];

            if (bb_count(b) == 2) {
                if (!(Segment[bb_lsb(b)][bb_msb(b)] & pos_pieces_cp(pos, color, KING)))
                    return false;
            } else if (bb_count(b) == 1) {
                if (pos_pieces_cp(pos, color, KING) & (File[FILE_A] | File[FILE_H]))
                    return false;
            } else if (b) {
                assert(bb_count(b) >= 3);
                return false;
            }
        }
    }

    // Verify ep square
    if (pos->epSquare != NB_SQUARE) {
        rank = rank_of(pos->epSquare);
        const int color = rank == RANK_3 ? WHITE : BLACK;

        if ((color == pos->turn) || (bb_test(pos_pieces(pos), pos->epSquare)) ||
            (rank != RANK_3 && rank != RANK_6) ||
            (!bb_test(pos_pieces_cp(pos, color, PAWN), pos->epSquare + push_inc(color))) ||
            (bb_test(pos_pieces(pos), pos->epSquare - push_inc(color))))
            return false;
    }

    finish(pos);
    return true;
}

// Get FEN string of position
void pos_get(const Position *pos, str_t *fen) {
    str_clear(fen);

    // Piece placement
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        int cnt = 0;

        for (int file = FILE_A; file <= FILE_H; file++) {
            const int square = square_from(rank, file);

            if (bb_test(pos_pieces(pos), square)) {
                if (cnt)
                    str_push(fen, (char)cnt + '0');

                str_push(fen, PieceLabel[pos_color_on(pos, square)][pos_piece_on(pos, square)]);
                cnt = 0;
            } else
                cnt++;
        }

        if (cnt)
            str_push(fen, (char)cnt + '0');

        str_push(fen, rank == RANK_1 ? ' ' : '/');
    }

    // Turn of play
    str_cat_c(fen, pos->turn == WHITE ? "w " : "b ");

    // Castling rights
    if (!pos->castleRooks)
        str_push(fen, '-');
    else {
        for (int color = WHITE; color <= BLACK; color++) {
            // Castling rook(s) for color
            const int king = pos_king_square(pos, color);
            const bitboard_t left =
                file_of(king) == FILE_A
                    ? 0
                    : pos->castleRooks & pos->byColor[color] & Ray[king][king + LEFT];
            const bitboard_t right =
                file_of(king) == FILE_H
                    ? 0
                    : pos->castleRooks & pos->byColor[color] & Ray[king][king + RIGHT];
            assert(!bb_several(left) && !bb_several(right));

            if (right)
                str_push(fen, pos->chess960 ? FileLabel[color][file_of(bb_lsb(right))]
                                            : PieceLabel[color][KING]);

            if (left)
                str_push(fen, pos->chess960 ? FileLabel[color][file_of(bb_lsb(left))]
                                            : PieceLabel[color][QUEEN]);
        }
    }

    // En passant and 50 move
    char epStr[3] = "";
    square_to_string(pos->epSquare, epStr);
    str_cat_fmt(fen, " %s %i %i", epStr, pos->rule50, pos->fullMove);
}

// Play a move on a position copy (original 'before' is untouched): pos = before + play(m)
void pos_move(Position *pos, const Position *before, move_t m) {
    *pos = *before;

    pos->rule50++;
    pos->epSquare = NB_SQUARE;

    const int us = pos->turn, them = opposite(us);
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    const int piece = pos_piece_on(pos, from);
    const int capture = pos_piece_on(pos, to);

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        assert(capture != KING);
        assert(!bb_test(pos->byColor[us], to) || (bb_test(pos->castleRooks, to) && piece == KING));
        pos->rule50 = 0;

        // Use pos_color_on() instead of them, because we could be playing a KxR castling here
        clear_square(pos, pos_color_on(pos, to), capture, to);

        // Capturing a rook alters corresponding castling right
        pos->castleRooks &= ~(1ULL << to);
    }

    if (piece <= QUEEN) {
        // Move piece
        clear_square(pos, us, piece, from);
        set_square(pos, us, piece, to);

        // Lose specific castling right (if not already lost)
        pos->castleRooks &= ~(1ULL << from);
    } else {
        // Move piece
        clear_square(pos, us, piece, from);
        set_square(pos, us, piece, to);

        if (piece == PAWN) {
            // reset rule50, and set epSquare
            const int push = push_inc(us);
            pos->rule50 = 0;

            // Set ep square upon double push, only if catpturably by enemy pawns
            if (to == from + 2 * push &&
                (PawnAttacks[us][from + push] & pos_pieces_cp(pos, them, PAWN)))
                pos->epSquare = (uint8_t)(from + push);

            // handle ep-capture and promotion
            if (to == before->epSquare)
                clear_square(pos, them, piece, to - push);
            else if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1) {
                clear_square(pos, us, piece, to);
                set_square(pos, us, prom, to);
            }
        } else if (piece == KING) {
            // Lose all castling rights
            pos->castleRooks &= ~Rank[us * RANK_8];

            // Castling
            if (bb_test(before->byColor[us], to)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(pos_piece_on(before, to) == ROOK);
                const int rank = rank_of(from);

                clear_square(pos, us, KING, to);
                set_square(pos, us, KING, square_from(rank, to > from ? FILE_G : FILE_C));
                set_square(pos, us, ROOK, square_from(rank, to > from ? FILE_F : FILE_D));
            }
        }
    }

    pos->turn = (uint8_t)them;
    pos->key ^= ZobristTurn;
    pos->key ^= ZobristEnPassant[before->epSquare] ^ ZobristEnPassant[pos->epSquare];
    pos->key ^= zobrist_castling(before->castleRooks ^ pos->castleRooks);
    pos->fullMove += pos->turn == WHITE;
    pos->lastMove = m;

    finish(pos);
}

// All pieces
bitboard_t pos_pieces(const Position *pos) {
    assert(!(pos->byColor[WHITE] & pos->byColor[BLACK]));
    return pos->byColor[WHITE] | pos->byColor[BLACK];
}

// Pieces of color 'color' and type 'piece'
bitboard_t pos_pieces_cp(const Position *pos, int color, int piece) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    return pos->byColor[color] & pos->byPiece[piece];
}

// Pieces of color 'color' and type 'p1' or 'p2'
bitboard_t pos_pieces_cpp(const Position *pos, int color, int p1, int p2) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(p1, NB_PIECE);
    BOUNDS(p2, NB_PIECE);
    return pos->byColor[color] & (pos->byPiece[p1] | pos->byPiece[p2]);
}

// Detect insufficient material configuration (draw by chess rules only)
bool pos_insufficient_material(const Position *pos) {
    return bb_count(pos_pieces(pos)) <= 3 && !pos->byPiece[PAWN] && !pos->byPiece[ROOK] &&
           !pos->byPiece[QUEEN];
}

// Square occupied by the king of color 'color'
int pos_king_square(const Position *pos, int color) {
    assert(bb_count(pos_pieces_cp(pos, color, KING)) == 1);
    return bb_lsb(pos_pieces_cp(pos, color, KING));
}

// Color of piece on square 'square'. Square is assumed to be occupied.
int pos_color_on(const Position *pos, int square) {
    assert(bb_test(pos_pieces(pos), square));
    return bb_test(pos->byColor[WHITE], square) ? WHITE : BLACK;
}

// Piece on square 'square'. NB_PIECE if empty.
int pos_piece_on(const Position *pos, int square) {
    BOUNDS(square, NB_SQUARE);

    for (int piece = KNIGHT; piece <= PAWN; piece++)
        if (bb_test(pos->byPiece[piece], square))
            return piece;

    return NB_PIECE;
}

bool pos_move_is_castling(const Position *pos, move_t m) {
    return bb_test(pos->byColor[pos->turn], move_to(m));
}

bool pos_move_is_tactical(const Position *pos, move_t m)
// Detect normal captures, castling (as KxR capture), en-passant captures, and promotions
{
    const int from = move_from(m), to = move_to(m);
    return bb_test(pos->byColor[WHITE] | pos->byColor[BLACK], to) ||
           (to == pos->epSquare && bb_test(pos_pieces_cp(pos, pos->turn, PAWN), from)) ||
           move_prom(m) <= QUEEN;
}

void pos_move_to_lan(const Position *pos, move_t m, str_t *lan) {
    const int from = move_from(m), prom = move_prom(m);
    int to = move_to(m);

    str_clear(lan);

    if (!(from | to | prom)) {
        str_cat_c(lan, "0000");
        return;
    }

    if (!pos->chess960 && pos_move_is_castling(pos, m))
        to = to > from ? from + 2 : from - 2; // e1h1 -> e1g1, e1a1 -> e1c1

    char fromStr[3] = "", toStr[3] = "";
    square_to_string(from, fromStr);
    square_to_string(to, toStr);
    str_cat_c(str_cat_c(lan, fromStr), toStr);

    if (prom < NB_PIECE)
        str_push(lan, PieceLabel[BLACK][prom]);
}

move_t pos_lan_to_move(const Position *pos, const char *lan) {
    const int prom =
        lan[4] ? (int)(strchr(PieceLabel[BLACK], lan[4]) - PieceLabel[BLACK]) : NB_PIECE;
    const int from = square_from(lan[1] - '1', lan[0] - 'a');
    int to = square_from(lan[3] - '1', lan[2] - 'a');

    if (!pos->chess960 && pos_piece_on(pos, from) == KING) {
        if (to == from + 2) // e1g1 -> e1h1
            to++;
        else if (to == from - 2) // e1c1 -> e1a1
            to -= 2;
    }

    return move_build(from, to, prom);
}

void pos_move_to_san(const Position *pos, move_t m, str_t *san)
// Converts a move to Standard Algebraic Notation. Note that the '+' (check) or '#' (checkmate)
// suffixes are not generated here.
{
    const int us = pos->turn;
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    const int piece = pos_piece_on(pos, from);

    str_clear(san);

    if (piece == PAWN) {
        str_push(san, (char)file_of(from) + 'a');

        if (pos_move_is_capture(pos, m) || to == pos->epSquare)
            str_push(str_push(san, 'x'), (char)file_of(to) + 'a');

        str_push(san, (char)rank_of(to) + '1');

        if (prom < NB_PIECE)
            str_push(str_push(san, '='), PieceLabel[WHITE][prom]);
    } else if (piece == KING) {
        if (pos_move_is_castling(pos, m))
            str_cat_c(san, to > from ? "O-O" : "O-O-O");
        else {
            str_push(san, 'K');

            if (pos_move_is_capture(pos, m))
                str_push(san, 'x');

            char toStr[3] = "";
            square_to_string(to, toStr);
            str_cat_c(san, toStr);
        }
    } else {
        str_push(san, PieceLabel[WHITE][piece]);

        // ** SAN disambiguation **

        // 1. Build a list of 'contesters', which are all our pieces of the same type that can also
        // reach the 'to' square.
        const bitboard_t pins = pos->pins;
        bitboard_t contesters = pos_pieces_cp(pos, us, piece);
        bb_clear(&contesters, from);

        if (piece == KNIGHT)
            // 1.1. Knights. Restrict to those within a knight jump of of 'to' that are not pinned.
            contesters &= KnightAttacks[to] & ~pins;
        else {
            // 1.2. Sliders
            assert(BISHOP <= piece && piece <= QUEEN);

            // 1.2.1. Restrict to those that can pseudo-legally reach the 'to' square.
            const bitboard_t occ = pos_pieces(pos);

            if (piece == BISHOP)
                contesters &= bb_bishop_attacks(to, occ);
            else if (piece == ROOK)
                contesters &= bb_rook_attacks(to, occ);
            else if (piece == QUEEN)
                contesters &= bb_bishop_attacks(to, occ) | bb_rook_attacks(to, occ);

            // 1.2.2. Remove pinned sliders, which, by sliding to the 'to' square, would escape
            // their pin-ray.
            bitboard_t pinnedContesters = contesters & pins;

            while (pinnedContesters) {
                const int pinnedContester = bb_pop_lsb(&pinnedContesters);

                if (!bb_test(Ray[pos_king_square(pos, us)][pinnedContester], to))
                    bb_clear(&contesters, pinnedContester);
            }
        }

        // 2. Use the contesters to disambiguate
        if (contesters) {
            // 2.1. Same file or rank, use either or both to disambiguate.
            if (bb_rook_attacks(from, 0) & contesters) {
                // 2.1.1. Contested rank. Use file to disambiguate
                if (Rank[rank_of(from)] & contesters)
                    str_push(san, (char)file_of(from) + 'a');

                // 2.1.2. Contested file. Use rank to disambiguate
                if (File[file_of(from)] & contesters)
                    str_push(san, (char)rank_of(from) + '1');
            } else
                // 2.2. No file or rank in common, use file to disambiguate.
                str_push(san, (char)file_of(from) + 'a');
        }

        if (pos_move_is_capture(pos, m))
            str_push(san, 'x');

        char toStr[3] = "";
        square_to_string(to, toStr);
        str_cat_c(san, toStr);
    }
}

// Prints the position in ASCII 'art' (for debugging)
void pos_print(const Position *pos) {
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        char line[] = ". . . . . . . .";

        for (int file = FILE_A; file <= FILE_H; file++) {
            const int square = square_from(rank, file);
            line[2 * file] = bb_test(pos_pieces(pos), square)
                                 ? PieceLabel[pos_color_on(pos, square)][pos_piece_on(pos, square)]
                             : square == pos->epSquare ? '*'
                                                       : '.';
        }

        puts(line);
    }

    scope(str_destroy) str_t fen = str_init();
    pos_get(pos, &fen);
    puts(fen.buf);

    scope(str_destroy) str_t lan = str_init();
    pos_move_to_lan(pos, pos->lastMove, &lan);
    printf("Last move: %s\n", lan.buf);
}

size_t pos_pack(const Position *pos, PackedPos *pp) {
    *pp = (PackedPos){.occ = pos_pieces(pos), .turn = pos->turn, .rule50 = pos->rule50};

    bitboard_t remaining = pp->occ;
    unsigned nibbleIdx = 0;

    while (remaining) {
        const int square = bb_pop_lsb(&remaining), color = pos_color_on(pos, square);

        // extended piece type: 6 normal ones + 2 extra to encode castling and en-passant
        int extPiece = pos_piece_on(pos, square);

        if (extPiece == ROOK && bb_test(pos->castleRooks, square))
            extPiece = PAWN + 1; // rook that can castle
        else if (extPiece == PAWN && pos->epSquare == square + push_inc(pos->turn))
            extPiece = PAWN + 2; // pawn that can be captured en-passant

        // nibble = 3 bits for extPiece + 1 bit for color
        const uint8_t nibble = (uint8_t)(2 * extPiece + color);
        pp->packedPieces[nibbleIdx / 2] |= nibbleIdx % 2 ? nibble << 4 : nibble;
        nibbleIdx++;
    }

    return offsetof(PackedPos, packedPieces) + (nibbleIdx + 1) / 2;
}
