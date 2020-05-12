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
#include <stdlib.h>
#include "game.h"
#include "gen.h"
#include "str.h"

static str_t uci_position_command(const Game *g)
// Builds a string of the form "position fen ... [moves ...]". Implements rule50 pruning: start from
// the last position that reset the rule50 counter, to reduce the move list to the minimum, without
// losing information.
{
    // Index of the starting FEN, where rule50 was last reset
    const int ply0 = max(g->ply - g->pos[g->ply].rule50, 0);

    str_t cmd = str_dup("position fen ");

    str_t fen = pos_get(&g->pos[ply0]);
    str_cat(&cmd, fen.buf);
    str_delete(&fen);

    if (ply0 < g->ply) {
        str_cat(&cmd, " moves");

        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            str_t lan = pos_move_to_lan(&g->pos[ply - 1], g->pos[ply].lastMove, g->chess960);
            str_cat(&cmd, " ", lan.buf);
            str_delete(&lan);
        }
    }

    return cmd;
}

int game_result(const Game *g, move_t *begin, move_t **end)
{
    const Position *pos = &g->pos[g->ply];

    *end = gen_all_moves(pos, begin);

    if (*end == begin)
        return pos->checkers ? RESULT_CHECKMATE : RESULT_STALEMATE;
    else if (pos->rule50 >= 100) {
        assert(pos->rule50 == 100);
        return RESULT_FIFTY_MOVES;
    } else if (pos_insufficient_material(pos))
        return RESULT_INSUFFICIENT_MATERIAL;
    else {
        // Scan for 3 repetitions
        int repetitions = 1;

        for (int i = 4; i <= pos->rule50 && i <= g->ply; i += 2)
            if (g->pos[g->ply - i].key == pos->key && ++repetitions >= 3)
                return RESULT_THREEFOLD;
    }

    return RESULT_NONE;
}

bool illegal_move(move_t move, const move_t *begin, const move_t *end)
{
    for (const move_t *m = begin; m != end; m++)
        if (*m == move)
            return false;

    return true;
}

Game game_new(bool chess960, const char *fen, unsigned nodes[2], int depth[2], int movetime[2])
{
    Game g;

    g.names[WHITE] = str_new();
    g.names[BLACK] = str_new();

    g.ply = 0;
    g.maxPly = 256;
    g.pos = malloc(g.maxPly * sizeof(Position));
    pos_set(&g.pos[0], fen);

    g.chess960 = chess960;
    g.result = RESULT_NONE;

    memcpy(g.nodes, nodes, 2 * sizeof(unsigned));
    memcpy(g.depth, depth, 2 * sizeof(int));
    memcpy(g.movetime, movetime, 2 * sizeof(int));

    return g;
}

void game_delete(Game *g)
{
    free(g->pos), g->pos = NULL;
    str_delete(&g->names[WHITE], &g->names[BLACK]);
}

void game_play(Game *g, const Engine *first, const Engine *second)
{
    const Engine *engines[2] = {first, second};  // more practical for loops

    for (int color = WHITE; color <= BLACK; color++)
        str_cpy(&g->names[color], engines[color ^ g->pos[0].turn]->name.buf);

    for (int i = 0; i < 2; i++) {
        if (g->chess960)
            engine_writeln(engines[i], "setoption name UCI_Chess960 value true");

        engine_writeln(engines[i], "ucinewgame");
        engine_sync(engines[i]);
    }

    move_t played = 0;
    str_t goCmd[2] = {str_dup("go"), str_dup("go")};

    for (int i = 0; i < 2; i++) {
        if (g->nodes[i])
            str_catf(&goCmd[i], " nodes %u", (int)g->nodes[i]);

        if (g->depth[i])
            str_catf(&goCmd[i], " depth %d", g->depth[i]);

        if (g->movetime[i])
            str_catf(&goCmd[i], " movetime %d", g->movetime[i]);
    }

    for (g->ply = 0; ; g->ply++) {
        if (g->ply >= g->maxPly) {
            g->maxPly *= 2;
            g->pos = realloc(g->pos, g->maxPly * sizeof(Position));
        }

        if (played)
            pos_move(&g->pos[g->ply], &g->pos[g->ply - 1], played);

        move_t moves[MAX_MOVES], *end;

        if ((g->result = game_result(g, moves, &end)))
            break;

        const int turn = g->ply % 2;
        const Engine *e = engines[turn];

        str_t posCmd = uci_position_command(g);
        engine_writeln(e, posCmd.buf);
        str_delete(&posCmd);

        engine_sync(e);

        engine_writeln(e, goCmd[turn].buf);
        str_t lan = engine_bestmove(e);
        played = pos_lan_to_move(&g->pos[g->ply], lan.buf, g->chess960);
        str_delete(&lan);

        if (illegal_move(played, moves, end)) {
            g->result = RESULT_ILLEGAL_MOVE;
            break;
        }
    }

    str_delete(&goCmd[0], &goCmd[1]);
    assert(g->result);
}

str_t game_decode_result(const Game *g, str_t *reason)
{
    str_t result = str_new();
    str_cpy(reason, "");  // default: termination by chess rules

    if (g->result == RESULT_NONE) {
        str_cpy(&result, "*");
        str_cpy(reason, "unterminated");
    } else if (g->result == RESULT_CHECKMATE) {
        str_cpy(&result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy(reason, "checkmate");
    } else if (g->result == RESULT_STALEMATE) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "stalemate");
    } else if (g->result == RESULT_THREEFOLD) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "3 repetitions");
    } else if (g->result == RESULT_FIFTY_MOVES) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "50 move rule");
    } else if (g->result ==RESULT_INSUFFICIENT_MATERIAL) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "insufficient material");
    } else if (g->result == RESULT_ILLEGAL_MOVE) {
        str_cpy(&result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy(reason, "illegal move");
    } else
        assert(false);

    return result;
}

str_t game_pgn(const Game *g)
// FIXME: Use SAN as required by PGN
{
    str_t pgn = str_new();

    str_cat(&pgn, "[White \"", g->names[WHITE].buf, "\"]\n");
    str_cat(&pgn, "[Black \"", g->names[BLACK].buf, "\"]\n");

    // Result in PGN format "1-0", "0-1", "1/2-1/2" (from white pov)
    str_t reason = str_new();
    str_t result = game_decode_result(g, &reason);
    str_cat(&pgn, "[Result \"", result.buf, "\"]\n");
    str_cat(&pgn, "[Termination \"", reason.buf, "\"]\n");

    str_t fen = pos_get(&g->pos[0]);
    str_cat(&pgn, "[FEN \"", fen.buf, "\"]\n");

    if (g->chess960)
        str_cat(&pgn, "[Variant \"Chess960\"]\n");

    str_catf(&pgn, "[PlyCount \"%d\"]\n\n", g->ply);

    for (int ply = 1; ply <= g->ply; ply++) {
        // Write move number
        if (g->pos[ply - 1].turn == WHITE || ply == 1)
            str_catf(&pgn, g->pos[ply - 1].turn == WHITE ? "%d. " : "%d.. ",
                g->pos[ply - 1].fullMove);

        // Prepare SAN base
        str_t san = pos_move_to_san(&g->pos[ply - 1], g->pos[ply].lastMove);

        // Append check markers to SAN
        if (g->pos[ply].checkers) {
            if (ply == g->ply && g->result == RESULT_CHECKMATE)
                str_putc(&san, '#');  // checkmate
            else
                str_putc(&san, '+');  // normal check
        }

        str_cat(&pgn, san.buf, ply % 10 == 0 ? "\n" : " ");
        str_delete(&san);
    }

    str_cat(&pgn, result.buf, "\n");
    str_delete(&result, &reason, &fen);
    return pgn;
}
