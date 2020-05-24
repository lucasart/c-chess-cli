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
#include "util.h"

static void uci_position_command(const Game *g, str_t *cmd)
// Builds a string of the form "position fen ... [moves ...]". Implements rule50 pruning: start from
// the last position that reset the rule50 counter, to reduce the move list to the minimum, without
// losing information.
{
    // Index of the starting FEN, where rule50 was last reset
    const int ply0 = max(g->ply - g->pos[g->ply].rule50, 0);

    str_cpy(cmd, "position fen ");
    str_t fen = pos_get(&g->pos[ply0]);
    str_cat_s(cmd, &fen);
    str_delete(&fen);

    if (ply0 < g->ply) {
        str_cat(cmd, " moves");

        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            str_t lan = pos_move_to_lan(&g->pos[ply - 1], g->pos[ply].lastMove, g->go.chess960);
            str_cat(cmd, " ", lan.buf);
            str_delete(&lan);
        }
    }
}

static void uci_go_command(Game *g, int ei, const int64_t timeLeft[2], str_t *cmd)
{
    str_cpy(cmd, "go");

    if (g->go.nodes[ei])
        str_cat_fmt(cmd, " nodes %U", g->go.nodes[ei]);

    if (g->go.depth[ei])
        str_cat_fmt(cmd, " depth %i", g->go.depth[ei]);

    if (g->go.movetime[ei])
        str_cat_fmt(cmd, " movetime %I", g->go.movetime[ei]);

    if (g->go.time[ei]) {
        const int color = g->pos[g->ply].turn;

        str_cat_fmt(cmd, " wtime %I winc %I btime %I binc %I",
            timeLeft[ei ^ color], g->go.increment[ei ^ color],
            timeLeft[ei ^ color ^ BLACK], g->go.increment[ei ^ color ^ BLACK]);
    }

    if (g->go.movestogo[ei])
        str_cat_fmt(cmd, " movestogo %i",
            g->go.movestogo[ei] - ((g->ply / 2) % g->go.movestogo[ei]));
}

static int game_apply_chess_rules(const Game *g, move_t *begin, move_t **end)
// Applies chess rules to generate legal moves, and determine the state of the game
{
    const Position *pos = &g->pos[g->ply];

    *end = gen_all_moves(pos, begin);

    if (*end == begin)
        return pos->checkers ? STATE_CHECKMATE : STATE_STALEMATE;
    else if (pos->rule50 >= 100) {
        assert(pos->rule50 == 100);
        return STATE_FIFTY_MOVES;
    } else if (pos_insufficient_material(pos))
        return STATE_INSUFFICIENT_MATERIAL;
    else {
        // Scan for 3 repetitions
        int repetitions = 1;

        for (int i = 4; i <= pos->rule50 && i <= g->ply; i += 2)
            if (g->pos[g->ply - i].key == pos->key && ++repetitions >= 3)
                return STATE_THREEFOLD;
    }

    return STATE_NONE;
}

static bool illegal_move(move_t move, const move_t *begin, const move_t *end)
{
    for (const move_t *m = begin; m != end; m++)
        if (*m == move)
            return false;

    return true;
}

Game game_new(const char *fen, const GameOptions *go)
{
    Game g;

    g.names[WHITE] = str_new();
    g.names[BLACK] = str_new();

    g.ply = 0;
    g.maxPly = 256;
    g.pos = malloc(g.maxPly * sizeof(Position));
    pos_set(&g.pos[0], fen);

    g.state = STATE_NONE;

    memcpy(&g.go, go, sizeof(*go));

    return g;
}

void game_delete(Game *g)
{
    free(g->pos), g->pos = NULL;
    str_delete(&g->names[WHITE], &g->names[BLACK]);
}

int game_play(Game *g, const Engine engines[2], Deadline *deadline, bool reverse)
// Play a game:
// - engines[reverse] plays the first move (which does not mean white, that depends on the FEN)
// - sets g->state value: see enum STATE_* codes
// - returns {-1,0,1} score from engines[0] pov.
{
    for (int color = WHITE; color <= BLACK; color++)
        str_cpy_s(&g->names[color], &engines[color ^ g->pos[0].turn ^ reverse].name);

    for (int i = 0; i < 2; i++) {
        if (g->go.chess960)
            engine_writeln(&engines[i], "setoption name UCI_Chess960 value true");

        engine_writeln(&engines[i], "ucinewgame");
        engine_sync(&engines[i], deadline);
    }

    str_t posCmd = str_new(), goCmd = str_new(), best = str_new();
    move_t played = 0;
    int drawPlyCount = 0;
    int resignCount[NB_COLOR] = {0};
    int ei;  // engines[ei] has the move
    int64_t timeLeft[2] = {g->go.time[0], g->go.time[1]};

    for (g->ply = 0; ; g->ply++) {
        if (g->ply >= g->maxPly) {
            g->maxPly *= 2;
            g->pos = realloc(g->pos, g->maxPly * sizeof(Position));
        }

        if (played)
            pos_move(&g->pos[g->ply], &g->pos[g->ply - 1], played);

        ei = (g->ply % 2) ^ reverse;  // engine[ei] has the move

        move_t moves[MAX_MOVES], *end;

        if ((g->state = game_apply_chess_rules(g, moves, &end)))
            break;

        uci_position_command(g, &posCmd);
        engine_writeln(&engines[ei], posCmd.buf);
        engine_sync(&engines[ei], deadline);

        // Prepare timeLeft[ei]
        if (g->go.movetime[ei])
            // movetime is special: discard movestogo, time, increment
            timeLeft[ei] = g->go.movetime[ei];
        else if (g->go.time[ei]) {
            // Always apply increment (can be zero)
            timeLeft[ei] += g->go.increment[ei];

            // movestogo specific clock reset event
            if (g->go.movestogo[ei] && g->ply > 1 && ((g->ply / 2) % g->go.movestogo[ei]) == 0)
                timeLeft[ei] += g->go.time[ei];
        } else
            // Only depth and/or nodes limit
            timeLeft[ei] = INT64_MAX / 2;  // HACK: system_msec() + timeLeft must not overflow

        uci_go_command(g, ei, timeLeft, &goCmd);
        engine_writeln(&engines[ei], goCmd.buf);

        int score;
        const bool ok = engine_bestmove(&engines[ei], &score, &timeLeft[ei], deadline, &best);

        if (!ok) {  // engine_bestmove() time out before parsing a bestmove
            g->state = STATE_TIME_LOSS;
            break;
        }

        played = pos_lan_to_move(&g->pos[g->ply], best.buf, g->go.chess960);

        if (illegal_move(played, moves, end)) {
            g->state = STATE_ILLEGAL_MOVE;
            break;
        }

        if ((g->go.time[ei] || g->go.movetime[ei]) && timeLeft[ei] < 0) {
            g->state = STATE_TIME_LOSS;
            break;
        }

        // Apply draw adjudication rule
        if (g->go.drawCount && abs(score) <= g->go.drawScore) {
            if (++drawPlyCount >= 2 * g->go.drawCount) {
                g->state = STATE_DRAW_ADJUDICATION;
                break;
            }
        } else
            drawPlyCount = 0;

        // Apply resign rule
        if (g->go.resignCount && score <= -g->go.resignScore) {
            if (++resignCount[ei] >= g->go.resignCount) {
                g->state = STATE_RESIGN;
                break;
            }
        } else
            resignCount[ei] = 0;
    }

    str_delete(&goCmd, &posCmd, &best);
    assert(g->state != STATE_NONE);

    if (g->state < STATE_SEPARATOR)
        return ei == 0 ? RESULT_LOSS : RESULT_WIN;
    else
        return RESULT_DRAW;
}

str_t game_decode_state(const Game *g, str_t *reason)
{
    str_t result = str_new();
    str_cpy(reason, "");  // default: termination by chess rules

    if (g->state == STATE_NONE) {
        str_cpy(&result, "*");
        str_cpy(reason, "unterminated");
    } else if (g->state == STATE_CHECKMATE) {
        str_cpy(&result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy(reason, "checkmate");
    } else if (g->state == STATE_STALEMATE) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "stalemate");
    } else if (g->state == STATE_THREEFOLD) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "3-fold repetition");
    } else if (g->state == STATE_FIFTY_MOVES) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "50 moves rule");
    } else if (g->state ==STATE_INSUFFICIENT_MATERIAL) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "insufficient material");
    } else if (g->state == STATE_ILLEGAL_MOVE) {
        str_cpy(&result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy(reason, "rules infraction");
    } else if (g->state == STATE_DRAW_ADJUDICATION) {
        str_cpy(&result, "1/2-1/2");
        str_cpy(reason, "adjudication");
    } else if (g->state == STATE_RESIGN) {
        str_cpy(&result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy(reason, "adjudication");
    } else if (g->state == STATE_TIME_LOSS) {
        str_cpy(&result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy(reason, "time forfeit");
    } else
        assert(false);

    return result;
}

str_t game_pgn(const Game *g)
{
    str_t pgn = str_new();

    str_cat_fmt(&pgn, "[White \"%S\"]\n", &g->names[WHITE]);
    str_cat_fmt(&pgn, "[Black \"%S\"]\n", &g->names[BLACK]);

    // Result in PGN format "1-0", "0-1", "1/2-1/2" (from white pov)
    str_t reason = str_new();
    str_t result = game_decode_state(g, &reason);
    str_cat_fmt(&pgn, "[Result \"%S\"]\n", &result);
    str_cat_fmt(&pgn, "[Termination \"%S\"]\n", &reason);

    str_t fen = pos_get(&g->pos[0]);
    str_cat_fmt(&pgn, "[FEN \"%S\"]\n", &fen);

    if (g->go.chess960)
        str_cat(&pgn, "[Variant \"Chess960\"]\n");

    str_cat_fmt(&pgn, "[PlyCount \"%i\"]\n\n", g->ply);

    for (int ply = 1; ply <= g->ply; ply++) {
        // Write move number
        if (g->pos[ply - 1].turn == WHITE || ply == 1)
            str_cat_fmt(&pgn, g->pos[ply - 1].turn == WHITE ? "%i. " : "%i... ",
                g->pos[ply - 1].fullMove);

        // Prepare SAN base
        str_t san = pos_move_to_san(&g->pos[ply - 1], g->pos[ply].lastMove);

        // Append check markers to SAN
        if (g->pos[ply].checkers) {
            if (ply == g->ply && g->state == STATE_CHECKMATE)
                str_putc(&san, '#');  // checkmate
            else
                str_putc(&san, '+');  // normal check
        }

        str_cat(&pgn, san.buf, ply % 10 == 0 ? "\n" : " ");
        str_delete(&san);
    }

    str_cat(&pgn, result.buf, "\n\n");
    str_delete(&result, &reason, &fen);
    return pgn;
}
