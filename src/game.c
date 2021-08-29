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
#include "util.h"
#include "vec.h"
#include <limits.h>
#include <math.h>

static bool is_mating(int score) { return score > INT16_MAX - 1024; }

static bool is_mated(int score) { return score < INT16_MIN + 1024; }

static bool is_mate(int score) { return is_mating(score) || is_mated(score); }

static void uci_position_command(const Game *g, str_t *cmd)
// Builds a string of the form "position fen ... [moves ...]". Implements rule50 pruning: start from
// the last position that reset the rule50 counter, to reduce the move list to the minimum, without
// losing information.
{
    // Index of the starting FEN, where rule50 was last reset
    const int ply0 = max(g->ply - g->vecPos[g->ply].rule50, 0);

    scope(str_destroy) str_t fen = str_init();
    pos_get(&g->vecPos[ply0], &fen);
    str_cpy_fmt(cmd, "position fen %S", fen);

    if (ply0 < g->ply) {
        scope(str_destroy) str_t lan = str_init();
        str_cat_c(cmd, " moves");

        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            pos_move_to_lan(&g->vecPos[ply - 1], g->vecPos[ply].lastMove, &lan);
            str_cat(str_push(cmd, ' '), lan);
        }
    }
}

static void uci_go_command(Game *g, const EngineOptions *eo[2], int ei, const int64_t timeLeft[2],
                           str_t *cmd) {
    str_cpy_c(cmd, "go");

    if (eo[ei]->nodes)
        str_cat_fmt(cmd, " nodes %I", eo[ei]->nodes);

    if (eo[ei]->depth)
        str_cat_fmt(cmd, " depth %i", eo[ei]->depth);

    if (eo[ei]->movetime)
        str_cat_fmt(cmd, " movetime %I", eo[ei]->movetime);

    if (eo[ei]->time || eo[ei]->increment) {
        const int color = g->vecPos[g->ply].turn;

        str_cat_fmt(cmd, " wtime %I winc %I btime %I binc %I", timeLeft[ei ^ color],
                    eo[ei ^ color]->increment, timeLeft[ei ^ color ^ BLACK],
                    eo[ei ^ color ^ BLACK]->increment);
    }

    if (eo[ei]->movestogo)
        str_cat_fmt(cmd, " movestogo %i", eo[ei]->movestogo - ((g->ply / 2) % eo[ei]->movestogo));
}

static int game_apply_chess_rules(const Game *g, move_t **moves)
// Applies chess rules to generate legal moves, and determine the state of the game
{
    const Position *pos = &g->vecPos[g->ply];

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
            if (g->vecPos[g->ply - i].key == pos->key && ++repetitions >= 3)
                return STATE_THREEFOLD;
    }

    return STATE_NONE;
}

static bool illegal_move(move_t move, const move_t *moves) {
    for (size_t i = 0; i < vec_size(moves); i++)
        if (moves[i] == move)
            return false;

    return true;
}

static Position resolve_pv(const Worker *w, const Game *g, const char *pv) {
    scope(str_destroy) str_t token = str_init();

    // Start with current position. We can't guarantee that the resolved position won't be in check,
    // but a valid one must be returned.
    Position resolved = g->vecPos[g->ply];

    Position p[2];
    p[0] = resolved;
    int idx = 0;
    move_t *vecMoves = vec_init_reserve(64, move_t);

    while ((pv = str_tok(pv, &token, " "))) {
        const move_t m = pos_lan_to_move(&p[idx], token.buf);

        // Only play tactical moves, stop on the first quiet move
        if (!pos_move_is_tactical(&p[idx], m))
            break;

        vecMoves = gen_all_moves(&p[idx], vecMoves);

        if (illegal_move(m, vecMoves)) {
            stdio_lock(stdout); // lock both stderr and stdout to prevent interleaving
            fprintf(stderr, "[%d] WARNING: Illegal move in PV '%s%s' from %s\n", threadId,
                    token.buf, pv, g->names[g->vecPos[g->ply].turn].buf);
            stdio_unlock(stdout);

            if (w->log)
                DIE_IF(fprintf(w->log, "WARNING: illegal move in PV '%s%s'\n", token.buf, pv) < 0);

            break;
        }

        pos_move(&p[(idx + 1) % 2], &p[idx], m);
        idx = (idx + 1) % 2;

        if (!p[idx].checkers)
            resolved = p[idx];
    }

    vec_destroy(vecMoves);
    return resolved;
}

Game game_init(int round, int game) {
    Game g = {.round = round,
              .game = game,
              .names = {str_init(), str_init()},
              .vecPos = vec_init(Position),
              .vecInfo = vec_init(Info),
              .vecSamples = vec_init(Sample)};

    vec_push(g.vecPos, (Position){0});
    return g;
}

bool game_load_fen(Game *g, const char *fen, int *color) {
    if (pos_set(&g->vecPos[0], fen, false)) {
        *color = g->vecPos[0].turn;
        return true;
    } else
        return false;
}

void game_destroy(Game *g) {
    vec_destroy(g->vecSamples);
    vec_destroy(g->vecInfo);
    vec_destroy(g->vecPos);

    str_destroy_n(&g->names[WHITE], &g->names[BLACK]);
}

int game_play(Worker *w, Game *g, const Options *o, const Engine engines[2],
              const EngineOptions *eo[2], bool reverse)
// Play a game:
// - engines[reverse] plays the first move (which does not mean white, that depends on the FEN)
// - sets g->state value: see enum STATE_* codes
// - returns RESULT_LOSS/DRAW/WIN from engines[0] pov
{
    for (int color = WHITE; color <= BLACK; color++)
        str_cpy(&g->names[color], engines[color ^ g->vecPos[0].turn ^ reverse].name);

    for (int i = 0; i < 2; i++) {
        if (g->vecPos[0].chess960) {
            if (engines[i].supportChess960)
                engine_writeln(w, &engines[i], "setoption name UCI_Chess960 value true");
            else
                DIE("[%d] '%s' does not support Chess960\n", threadId, engines[i].name.buf);
        }

        engine_newgame(w, &engines[i]);
        engine_sync(w, &engines[i]);
    }

    scope(str_destroy) str_t cmd = str_init(), best = str_init();
    move_t played = 0;
    int drawPlyCount = 0;
    int resignCount[NB_COLOR] = {0};
    int ei = reverse; // engines[ei] has the move
    int64_t timeLeft[2] = {eo[0]->time, eo[1]->time};
    scope(str_destroy) str_t pv = str_init();
    move_t *vecLegalMoves = vec_init_reserve(64, move_t);

    for (g->ply = 0;; ei = 1 - ei, g->ply++) {
        if (played)
            pos_move(&g->vecPos[g->ply], &g->vecPos[g->ply - 1], played);

        if ((g->state = game_apply_chess_rules(g, &vecLegalMoves)))
            break;

        uci_position_command(g, &cmd);
        engine_writeln(w, &engines[ei], cmd.buf);
        engine_sync(w, &engines[ei]);

        // Prepare timeLeft[ei]
        if (eo[ei]->movetime)
            // movetime is special: discard movestogo, time, increment
            timeLeft[ei] = eo[ei]->movetime;
        else if (eo[ei]->time || eo[ei]->increment) {
            // Always apply increment (can be zero)
            timeLeft[ei] += eo[ei]->increment;

            // movestogo specific clock reset event
            if (eo[ei]->movestogo && g->ply > 1 && ((g->ply / 2) % eo[ei]->movestogo) == 0)
                timeLeft[ei] += eo[ei]->time;
        } else
            // Only depth and/or nodes limit
            timeLeft[ei] = INT64_MAX / 2; // HACK: system_msec() + timeLeft must not overflow

        uci_go_command(g, eo, ei, timeLeft, &cmd);
        engine_writeln(w, &engines[ei], cmd.buf);

        Info info = {0};
        const bool ok = engine_bestmove(w, &engines[ei], &timeLeft[ei], &best, &pv, &info);
        vec_push(g->vecInfo, info);

        // Parses the last PV sent. An invalid PV is not fatal, but logs some warnings. Keep track
        // of the resolved position, which is the last in the PV that is not in check (or the
        // current one if that's impossible).
        Position resolved = resolve_pv(w, g, pv.buf);

        if (!ok) { // engine_bestmove() time out before parsing a bestmove
            g->state = STATE_TIME_LOSS;
            break;
        }

        played = pos_lan_to_move(&g->vecPos[g->ply], best.buf);

        if (illegal_move(played, vecLegalMoves)) {
            g->state = STATE_ILLEGAL_MOVE;
            break;
        }

        if ((eo[ei]->time || eo[ei]->increment || eo[ei]->movetime) && timeLeft[ei] < 0) {
            g->state = STATE_TIME_LOSS;
            break;
        }

        // Apply draw adjudication rule
        if (o->drawCount && abs(info.score) <= o->drawScore) {
            if (++drawPlyCount >= 2 * o->drawCount && g->ply / 2 + 1 >= o->drawNumber) {
                g->state = STATE_DRAW_ADJUDICATION;
                break;
            }
        } else
            drawPlyCount = 0;

        // Apply resign rule
        if (o->resignCount && info.score <= -o->resignScore) {
            if (++resignCount[ei] >= o->resignCount && g->ply / 2 + 1 >= o->resignNumber) {
                g->state = STATE_RESIGN;
                break;
            }
        } else
            resignCount[ei] = 0;

        // Write sample: position (compactly encoded) + score
        if (o->sp.fileName.len && !(o->sp.resolve && is_mate(info.score)) &&
            prngf(&w->seed) <= o->sp.freq * exp(-o->sp.decay * g->vecPos[g->ply].rule50)) {
            Sample sample = (Sample){
                .pos = o->sp.resolve ? resolved : g->vecPos[g->ply],
                .score = sample.pos.turn == g->vecPos[g->ply].turn ? info.score : -info.score,
                .result = NB_RESULT // mark as invalid for now, computed after the game
            };

            // Record sample, except if resolvePv=true and the position is in check (becuase PV
            // resolution couldn't avoid it), in which case the sample is discarded.
            if (!o->sp.resolve || !sample.pos.checkers)
                vec_push(g->vecSamples, sample);
        }

        vec_push(g->vecPos, (Position){0});
    }

    assert(g->state != STATE_NONE);
    vec_destroy(vecLegalMoves);

    // Signed result from white's pov: -1 (loss), 0 (draw), +1 (win)
    const int wpov =
        g->state < STATE_SEPARATOR
            ? (g->vecPos[g->ply].turn == WHITE ? RESULT_LOSS : RESULT_WIN) // lost from turn's pov
            : RESULT_DRAW;

    for (size_t i = 0; i < vec_size(g->vecSamples); i++)
        g->vecSamples[i].result = g->vecSamples[i].pos.turn == WHITE ? wpov : 2 - wpov;

    return g->state < STATE_SEPARATOR
               ? (ei == 0 ? RESULT_LOSS : RESULT_WIN) // engine on the move has lost
               : RESULT_DRAW;
}

void game_decode_state(const Game *g, str_t *result, str_t *reason) {
    str_cpy_c(result, "1/2-1/2");
    str_clear(reason);

    if (g->state == STATE_NONE) {
        str_cpy_c(result, "*");
        str_cpy_c(reason, "unterminated");
    } else if (g->state == STATE_CHECKMATE) {
        str_cpy_c(result, g->vecPos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "checkmate");
    } else if (g->state == STATE_STALEMATE)
        str_cpy_c(reason, "stalemate");
    else if (g->state == STATE_THREEFOLD)
        str_cpy_c(reason, "3-fold repetition");
    else if (g->state == STATE_FIFTY_MOVES)
        str_cpy_c(reason, "50 moves rule");
    else if (g->state == STATE_INSUFFICIENT_MATERIAL)
        str_cpy_c(reason, "insufficient material");
    else if (g->state == STATE_ILLEGAL_MOVE) {
        str_cpy_c(result, g->vecPos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "rules infraction");
    } else if (g->state == STATE_DRAW_ADJUDICATION)
        str_cpy_c(reason, "adjudication");
    else if (g->state == STATE_RESIGN) {
        str_cpy_c(result, g->vecPos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "adjudication");
    } else if (g->state == STATE_TIME_LOSS) {
        str_cpy_c(result, g->vecPos[g->ply].turn == WHITE ? "0-1" : "1-0");
        str_cpy_c(reason, "time forfeit");
    } else
        assert(false);
}

void game_export_pgn(const Game *g, int verbosity, str_t *out) {
    str_cpy_fmt(out, "[Round \"%i.%i\"]\n", g->round + 1, g->game + 1);
    str_cat_fmt(out, "[White \"%S\"]\n", g->names[WHITE]);
    str_cat_fmt(out, "[Black \"%S\"]\n", g->names[BLACK]);

    // Result in PGN format "1-0", "0-1", "1/2-1/2" (from white pov)
    scope(str_destroy) str_t result = str_init(), reason = str_init();
    game_decode_state(g, &result, &reason);
    str_cat_fmt(out, "[Result \"%S\"]\n", result);
    str_cat_fmt(out, "[Termination \"%S\"]\n", reason);

    scope(str_destroy) str_t fen = str_init();
    pos_get(&g->vecPos[0], &fen);
    str_cat_fmt(out, "[FEN \"%S\"]\n", fen);

    if (g->vecPos[0].chess960)
        str_cat_c(out, "[Variant \"Chess960\"]\n");

    str_cat_fmt(out, "[PlyCount \"%i\"]\n", g->ply);
    scope(str_destroy) str_t san = str_init();

    if (verbosity > 0) {
        // Print the moves
        str_push(out, '\n');

        const int pliesPerLine = verbosity == 2 ? 6 : verbosity == 3 ? 5 : 16;

        for (int ply = 1; ply <= g->ply; ply++) {
            // Write move number
            if (g->vecPos[ply - 1].turn == WHITE || ply == 1)
                str_cat_fmt(out, g->vecPos[ply - 1].turn == WHITE ? "%i. " : "%i... ",
                            g->vecPos[ply - 1].fullMove);

            // Append SAN move
            pos_move_to_san(&g->vecPos[ply - 1], g->vecPos[ply].lastMove, &san);
            str_cat(out, san);

            // Append check marker
            if (g->vecPos[ply].checkers) {
                if (ply == g->ply && g->state == STATE_CHECKMATE)
                    str_push(out, '#'); // checkmate
                else
                    str_push(out, '+'); // normal check
            }

            // Write PGN comment
            const int depth = g->vecInfo[ply - 1].depth, score = g->vecInfo[ply - 1].score;

            if (verbosity == 2) {
                if (is_mating(score))
                    str_cat_fmt(out, " {M%i/%i}", INT16_MAX - score, depth);
                else if (is_mated(score))
                    str_cat_fmt(out, " {-M%i/%i}", score - INT16_MIN, depth);
                else
                    str_cat_fmt(out, " {%i/%i}", score, depth);
            } else if (verbosity == 3) {
                const int64_t time = g->vecInfo[ply - 1].time;

                if (is_mating(score))
                    str_cat_fmt(out, " {M%i/%i %Ims}", INT16_MAX - score, depth, time);
                else if (is_mated(score))
                    str_cat_fmt(out, " {-M%i/%i %Ims}", score - INT16_MIN, depth, time);
                else
                    str_cat_fmt(out, " {%i/%i %Ims}", score, depth, time);
            }

            // Append delimiter
            str_push(out, ply % pliesPerLine == 0 ? '\n' : ' ');
        }
    }

    str_cat_c(str_cat(out, result), "\n\n");
}

static void game_export_samples_csv(const Game *g, FILE *out) {
    scope(str_destroy) str_t fen = str_init();

    for (size_t i = 0; i < vec_size(g->vecSamples); i++) {
        pos_get(&g->vecSamples[i].pos, &fen);
        fprintf(out, "%s,%d,%d\n", fen.buf, g->vecSamples[i].score, g->vecSamples[i].result);
    }
}

static void game_export_samples_bin(const Game *g, FILE *out) {
    for (size_t i = 0; i < vec_size(g->vecSamples); i++) {
        PackedPos packed = {0};
        const size_t bytes = pos_pack(&g->vecSamples[i].pos, &packed);

        fwrite(&packed, bytes, 1, out);
        fwrite(&g->vecSamples[i].score, sizeof(g->vecSamples[i].score), 1, out);
        fwrite(&g->vecSamples[i].result, sizeof(g->vecSamples[i].result), 1, out);
    }
}

void game_export_samples(const Game *g, FILE *out, bool bin) {
    stdio_lock(out);

    if (bin)
        game_export_samples_bin(g, out);
    else
        game_export_samples_csv(g, out);

    stdio_unlock(out);
}
