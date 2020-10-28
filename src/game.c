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

    scope(str_del) str_t fen = str_new();
    pos_get(&g->pos[ply0], &fen, g->sfen);
    str_cpy_fmt(cmd, "position fen %S", fen);

    if (ply0 < g->ply) {
        scope(str_del) str_t lan = str_new();
        str_cat_c(cmd, " moves");

        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            pos_move_to_lan(&g->pos[ply - 1], g->pos[ply].lastMove, &lan);
            str_cat(str_push(cmd, ' '), lan);
        }
    }
}

static void uci_go_command(Game *g, const EngineOptions *eo[2], int ei, const int64_t timeLeft[2],
    str_t *cmd)
{
    str_cpy_c(cmd, "go");

    if (eo[ei]->nodes)
        str_cat_fmt(cmd, " nodes %I", eo[ei]->nodes);

    if (eo[ei]->depth)
        str_cat_fmt(cmd, " depth %i", eo[ei]->depth);

    if (eo[ei]->movetime)
        str_cat_fmt(cmd, " movetime %I", eo[ei]->movetime);

    if (eo[ei]->time) {
        const int color = g->pos[g->ply].turn;

        str_cat_fmt(cmd, " wtime %I winc %I btime %I binc %I",
            timeLeft[ei ^ color], eo[ei ^ color]->increment,
            timeLeft[ei ^ color ^ BLACK], eo[ei ^ color ^ BLACK]->increment);
    }

    if (eo[ei]->movestogo)
        str_cat_fmt(cmd, " movestogo %i",
            eo[ei]->movestogo - ((g->ply / 2) % eo[ei]->movestogo));
}

static int game_apply_chess_rules(const Game *g, move_t **moves)
// Applies chess rules to generate legal moves, and determine the state of the game
{
    const Position *pos = &g->pos[g->ply];

    *moves = gen_all_moves(pos, *moves);

    if (!vec_size(*moves))
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

static bool illegal_move(move_t move, const move_t *moves)
{
    for (size_t i = 0; i < vec_size(moves); i++)
        if (moves[i] == move)
            return false;

    return true;
}

static Position resolve_pv(const Worker *w, const Game *g, const char *pv)
{
    scope(str_del) str_t token = str_new();
    const char *tail = pv;

    // Start with current position. We can't guarantee that the resolved position won't be in check,
    // but a valid one must be returned.
    Position resolved = g->pos[g->ply];

    Position p[2];
    p[0] = resolved;
    int idx = 0;
    move_t *moves = vec_new_reserve(64, move_t);
    scope(str_del) str_t fen = str_new();

    while ((tail = str_tok(tail, &token, " "))) {
        const move_t m = pos_lan_to_move(&p[idx], token.buf);
        moves = gen_all_moves(&p[idx], moves);

        if (illegal_move(m, moves)) {
            if (w->log) {
                pos_get(&g->pos[g->ply], &fen, g->sfen);
                DIE_IF(w->id, fprintf(w->log, "WARNING: invalid PV\n") < 0);
                DIE_IF(w->id, fprintf(w->log, "\tfen: '%s'\n", fen.buf) < 0);
                DIE_IF(w->id, fprintf(w->log, "\tpv: '%s'\n", pv) < 0);
                DIE_IF(w->id, fprintf(w->log, "\t'%s%s' starts with an illegal move\n", token.buf,
                    tail) < 0);
            }

            break;
        }

        pos_move(&p[(idx + 1) % 2], &p[idx], m);
        idx = (idx + 1) % 2;

        if (!p[idx].checkers)
            resolved = p[idx];
    }

    vec_del(moves);
    return resolved;
}

Game game_new(void)
{
    Game g = {0};

    g.names[WHITE] = str_new();
    g.names[BLACK] = str_new();

    g.pos = vec_new(Position);
    g.samples = vec_new(Sample);

    return g;
}

bool game_load_fen(Game *g, const char *fen, int *color)
{
    vec_push(g->pos, (Position){0});

    if (pos_set(&g->pos[0], fen, false, &g->sfen)) {
        *color = g->pos[0].turn;
        return true;
    } else
        return false;
}

void game_del(Game *g)
{
    assert(g);

    vec_del(g->pos);
    vec_del(g->samples);
    str_del_n(&g->names[WHITE], &g->names[BLACK]);
}

int game_play(Worker *w, Game *g, const GameOptions *go, const Engine engines[2],
    const EngineOptions *eo[2], bool reverse)
// Play a game:
// - engines[reverse] plays the first move (which does not mean white, that depends on the FEN)
// - sets g->state value: see enum STATE_* codes
// - returns RESULT_LOSS/DRAW/WIN from engines[0] pov
{
    assert(w && g && go);

    for (int color = WHITE; color <= BLACK; color++)
        str_cpy(&g->names[color], engines[color ^ g->pos[0].turn ^ reverse].name);

    for (int i = 0; i < 2; i++) {
        if (g->pos[0].chess960) {
            if (engines[i].supportChess960)
                engine_writeln(w, &engines[i], "setoption name UCI_Chess960 value true");
            else
                DIE("[%d] '%s' does not support Chess960\n", w->id, engines[i].name.buf);
        }

        engine_writeln(w, &engines[i], "ucinewgame");
        engine_sync(w, &engines[i]);
    }

    scope(str_del) str_t posCmd = str_new(), goCmd = str_new(), best = str_new();
    move_t played = 0;
    int drawPlyCount = 0;
    int resignCount[NB_COLOR] = {0};
    int ei = reverse;  // engines[ei] has the move
    int64_t timeLeft[2] = {eo[0]->time, eo[1]->time};
    scope(str_del) str_t pv = str_new();
    move_t *legalMoves = vec_new_reserve(64, move_t);

    for (g->ply = 0; ; ei = 1 - ei, g->ply++) {
        if (played)
            pos_move(&g->pos[g->ply], &g->pos[g->ply - 1], played);

        if ((g->state = game_apply_chess_rules(g, &legalMoves)))
            break;

        uci_position_command(g, &posCmd);
        engine_writeln(w, &engines[ei], posCmd.buf);
        engine_sync(w, &engines[ei]);

        // Prepare timeLeft[ei]
        if (eo[ei]->movetime)
            // movetime is special: discard movestogo, time, increment
            timeLeft[ei] = eo[ei]->movetime;
        else if (eo[ei]->time) {
            // Always apply increment (can be zero)
            timeLeft[ei] += eo[ei]->increment;

            // movestogo specific clock reset event
            if (eo[ei]->movestogo && g->ply > 1 && ((g->ply / 2) % eo[ei]->movestogo) == 0)
                timeLeft[ei] += eo[ei]->time;
        } else
            // Only depth and/or nodes limit
            timeLeft[ei] = INT64_MAX / 2;  // HACK: system_msec() + timeLeft must not overflow

        uci_go_command(g, eo, ei, timeLeft, &goCmd);
        engine_writeln(w, &engines[ei], goCmd.buf);

        int score = 0;
        const bool ok = engine_bestmove(w, &engines[ei], &score, &timeLeft[ei], &best, &pv);

        // Parses the last PV sent. An invalid PV is not fatal, but logs some warnings. Keep track
        // of the resolved position, which is the last in the PV that is not in check (or the
        // current one if that's impossible).
        Position resolved = resolve_pv(w, g, pv.buf);

        if (!ok) {  // engine_bestmove() time out before parsing a bestmove
            g->state = STATE_TIME_LOSS;
            break;
        }

        played = pos_lan_to_move(&g->pos[g->ply], best.buf);

        if (illegal_move(played, legalMoves)) {
            g->state = STATE_ILLEGAL_MOVE;
            break;
        }

        if ((eo[ei]->time || eo[ei]->movetime) && timeLeft[ei] < 0) {
            g->state = STATE_TIME_LOSS;
            break;
        }

        // Apply draw adjudication rule
        if (go->drawCount && abs(score) <= go->drawScore) {
            if (++drawPlyCount >= 2 * go->drawCount) {
                g->state = STATE_DRAW_ADJUDICATION;
                break;
            }
        } else
            drawPlyCount = 0;

        // Apply resign rule
        if (go->resignCount && score <= -go->resignScore) {
            if (++resignCount[ei] >= go->resignCount) {
                g->state = STATE_RESIGN;
                break;
            }
        } else
            resignCount[ei] = 0;

        // Write sample: position (compactly encoded) + score
        if (prngf(&w->seed) <= go->sampleFrequency) {
            Sample sample = {
                .pos = go->sampleResolvePv ? resolved : g->pos[g->ply],
                .score = score,
                .result = NB_RESULT // unknown yet (use invalid state for now)
            };

            // Record sample, except if resolvePv=true and the position is in check (becuase PV
            // resolution couldn't avoid it), in which case the sample is discarded.
            if (!go->sampleResolvePv || !sample.pos.checkers)
                vec_push(g->samples, sample);
        }

        vec_push(g->pos, (Position){0});
    }

    assert(g->state != STATE_NONE);
    vec_del(legalMoves);

    // Signed result from white's pov: -1 (loss), 0 (draw), +1 (win)
    const int wpov = g->state < STATE_SEPARATOR
        ? (g->pos[g->ply].turn == WHITE ? RESULT_LOSS : RESULT_WIN)  // lost from turn's pov
        : RESULT_DRAW;

    for (size_t i = 0; i < vec_size(g->samples); i++)
        g->samples[i].result = g->samples[i].pos.turn == WHITE ? wpov : 2 - wpov;

    return g->state < STATE_SEPARATOR
        ? (ei == 0 ? RESULT_LOSS : RESULT_WIN)  // engine on the move has lost
        : RESULT_DRAW;
}

void game_decode_state(const Game *g, str_t *result, str_t *reason)
{
    assert(g && result && reason);
    str_cpy_c(result, "1/2-1/2");
    str_clear(reason);

    if (g->state == STATE_NONE) {
        str_cpy_c(result, "*");
        str_cpy_c(reason, "unterminated");
    } else if (g->state == STATE_CHECKMATE) {
        str_cpy_c(result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "checkmate");
    } else if (g->state == STATE_STALEMATE)
        str_cpy_c(reason, "stalemate");
    else if (g->state == STATE_THREEFOLD)
        str_cpy_c(reason, "3-fold repetition");
    else if (g->state == STATE_FIFTY_MOVES)
        str_cpy_c(reason, "50 moves rule");
    else if (g->state ==STATE_INSUFFICIENT_MATERIAL)
        str_cpy_c(reason, "insufficient material");
    else if (g->state == STATE_ILLEGAL_MOVE) {
        str_cpy_c(result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "rules infraction");
    } else if (g->state == STATE_DRAW_ADJUDICATION)
        str_cpy_c(reason, "adjudication");
    else if (g->state == STATE_RESIGN) {
        str_cpy_c(result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "adjudication");
    } else if (g->state == STATE_TIME_LOSS) {
        str_cpy_c(result, g->pos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "time forfeit");
    } else
        assert(false);
}

void game_pgn(const Game *g, str_t *pgn)
{
    str_cpy_fmt(pgn, "[White \"%S\"]\n", g->names[WHITE]);
    str_cat_fmt(pgn, "[Black \"%S\"]\n", g->names[BLACK]);

    // Result in PGN format "1-0", "0-1", "1/2-1/2" (from white pov)
    scope(str_del) str_t result = str_new(), reason = str_new();
    game_decode_state(g, &result, &reason);
    str_cat_fmt(pgn, "[Result \"%S\"]\n", result);
    str_cat_fmt(pgn, "[Termination \"%S\"]\n", reason);

    scope(str_del) str_t fen = str_new();
    pos_get(&g->pos[0], &fen, g->sfen);
    str_cat_fmt(pgn, "[FEN \"%S\"]\n", fen);

    if (g->pos[0].chess960)
        str_cat_c(pgn, "[Variant \"Chess960\"]\n");

    str_cat_fmt(pgn, "[PlyCount \"%i\"]\n\n", g->ply);
    scope(str_del) str_t san = str_new();

    for (int ply = 1; ply <= g->ply; ply++) {
        // Write move number
        if (g->pos[ply - 1].turn == WHITE || ply == 1)
            str_cat_fmt(pgn, g->pos[ply - 1].turn == WHITE ? "%i. " : "%i... ",
                g->pos[ply - 1].fullMove);

        // Append SAN move
        pos_move_to_san(&g->pos[ply - 1], g->pos[ply].lastMove, &san);
        str_cat(pgn, san);

        // Append check marker
        if (g->pos[ply].checkers) {
            if (ply == g->ply && g->state == STATE_CHECKMATE)
                str_push(pgn, '#');  // checkmate
            else
                str_push(pgn, '+');  // normal check
        }

        // Append delimiter
        str_push(pgn, ply % 10 == 0 ? '\n' : ' ');
    }

    str_cat_c(str_cat(pgn, result), "\n\n");
}
