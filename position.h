#pragma once
#include "bitboard.h"

enum {MATE = 32000};

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

// Max number of bytes needed to store a FEN
enum {MAX_FEN = 64 + 8 + 2 + 5 + 3 + 4 + 4 + 1};

typedef struct {
    bitboard_t byColor[NB_COLOR];  // eg. byColor[WHITE] = squares occupied by white's army
    bitboard_t byPiece[NB_PIECE];  // eg. byPiece[KNIGHT] = squares occupied by knights (any color)
    bitboard_t castleRooks;  // rooks with castling rights (eg. A1, A8, H1, H8 in start pos)
    bitboard_t attacked;  // squares attacked by enemy
    bitboard_t checkers;  // if in check, enemy piece(s) giving check(s), otherwise empty
    uint64_t key;  // hash key encoding all information of the position (except rule50)
    uint8_t pieceOn[NB_SQUARE];  // eg. pieceOn[D1] = QUEEN in start pos
    int turn;  // turn of play (WHITE or BLACK)
    int epSquare;  // en-passant square (NB_SQUARE if none)
    int rule50;  // ply counter for 50-move rule, ranging from 0 to 100 = draw (unless mated)
} Position;

extern const char *PieceLabel[NB_COLOR];

void square_to_string(int square, char *str);
int string_to_square(const char *str);

void pos_set(Position *pos, const char *fen);
void pos_get(const Position *pos, char *fen);

void pos_move(Position *pos, const Position *before, move_t m);
void pos_switch(Position *pos, const Position *before);

bitboard_t pos_pieces(const Position* pos);
bitboard_t pos_pieces_cp(const Position *pos, int color, int piece);
bitboard_t pos_pieces_cpp(const Position *pos, int color, int p1, int p2);

bitboard_t pos_ep_square_bb(const Position *pos);
bool pos_insufficient_material(const Position *pos);
int pos_king_square(const Position *pos, int color);
int pos_color_on(const Position *pos, int square);
int pos_piece_on(const Position *pos, int square);
bitboard_t pos_attackers_to(const Position *pos, int square, bitboard_t occ);
bitboard_t calc_pins(const Position *pos);

bool pos_move_is_capture(const Position *pos, move_t m);
bool pos_move_is_castling(const Position *pos, move_t m);
void pos_move_to_string(const Position *pos, move_t m, char *str, bool chess960);
move_t pos_string_to_move(const Position *pos, const char *str, bool chess960);
int pos_see(const Position *pos, move_t m);

void pos_print(const Position *pos);
