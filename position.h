#pragma once
#include "bitboard.h"
#include "str.h"

enum {MATE = 32000};

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

typedef struct {
    bitboard_t byColor[NB_COLOR];  // eg. byColor[WHITE] = squares occupied by white's army
    bitboard_t byPiece[NB_PIECE];  // eg. byPiece[KNIGHT] = squares occupied by knights (any color)
    bitboard_t castleRooks;  // rooks with castling rights (eg. A1, A8, H1, H8 in start pos)
    uint64_t key;  // hash key encoding all information of the position (except rule50)
    uint64_t attacked;  // squares attacked by enemy
    uint64_t checkers;  // if in check, enemy piece(s) giving check(s), otherwise empty
    uint64_t pins;  // pinned pieces for the side to move
    move_t lastMove;  // last move played
    uint16_t fullMove;  // full move number, starts at 1
    uint8_t turn;  // turn of play (WHITE or BLACK)
    uint8_t epSquare;  // en-passant square (NB_SQUARE if none)
    uint8_t rule50;  // ply counter for 50-move rule, ranging from 0 to 100 = draw (unless mated)
} Position;

void pos_set(Position *pos, const char *fen);
str_t pos_get(const Position *pos);
void pos_move(Position *pos, const Position *before, move_t m);

bitboard_t pos_pieces(const Position* pos);
bitboard_t pos_pieces_cp(const Position *pos, int color, int piece);
bitboard_t pos_pieces_cpp(const Position *pos, int color, int p1, int p2);

bool pos_insufficient_material(const Position *pos);
int pos_king_square(const Position *pos, int color);
int pos_color_on(const Position *pos, int square);
int pos_piece_on(const Position *pos, int square);

bool pos_move_is_castling(const Position *pos, move_t m);
str_t pos_move_to_lan(const Position *pos, move_t m, bool chess960);
str_t pos_move_to_san(const Position *pos, move_t m);
move_t pos_string_to_move(const Position *pos, const char *str, bool chess960);

void pos_print(const Position *pos);
