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
#pragma once
#include "bitboard.h"
#include "str.h"

typedef struct {
    bitboard_t byColor[NB_COLOR]; // eg. byColor[WHITE] = squares occupied by white's army
    bitboard_t byPiece[NB_PIECE]; // eg. byPiece[KNIGHT] = squares occupied by knights (any color)
    bitboard_t castleRooks;       // rooks with castling rights (eg. A1, A8, H1, H8 in start pos)
    uint64_t key;      // hash key encoding all information of the position (except rule50)
    uint64_t attacked; // squares attacked by enemy
    uint64_t checkers; // if in check, enemy piece(s) giving check(s), otherwise empty
    uint64_t pins;     // pinned pieces for the side to move
    move_t lastMove;   // last move played
    uint16_t fullMove; // full move number, starts at 1
    uint8_t turn;      // turn of play (WHITE or BLACK)
    uint8_t epSquare;  // en-passant square (NB_SQUARE if none)
    uint8_t rule50;    // ply counter for 50-move rule, ranging from 0 to 100 = draw (unless mated)
    bool chess960;     // for move<->string conversions ("e1h1" if chess960 else "e1g1")
} Position;

typedef struct {
    bitboard_t occ;
    uint8_t turn : 1, rule50 : 7;
    uint8_t packedPieces[16];
} PackedPos;

bool pos_set(Position *pos, const char *fen, bool force960);
void pos_get(const Position *pos, str_t *fen);
void pos_move(Position *pos, const Position *before, move_t m);

bitboard_t pos_pieces(const Position *pos);
bitboard_t pos_pieces_cp(const Position *pos, int color, int piece);
bitboard_t pos_pieces_cpp(const Position *pos, int color, int p1, int p2);

bool pos_insufficient_material(const Position *pos);
int pos_king_square(const Position *pos, int color);
int pos_color_on(const Position *pos, int square);
int pos_piece_on(const Position *pos, int square);

bool pos_move_is_castling(const Position *pos, move_t m);
bool pos_move_is_tactical(const Position *pos, move_t m);

void pos_move_to_lan(const Position *pos, move_t m, str_t *lan);
void pos_move_to_san(const Position *pos, move_t m, str_t *san);
move_t pos_lan_to_move(const Position *pos, const char *lan);

void pos_print(const Position *pos);

size_t pos_pack(const Position *pos, PackedPos *pp);
