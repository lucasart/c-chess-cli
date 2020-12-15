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
#include <limits.h>
#include "game.h"
#include "gen.h"
#include "util.h"
#include "vec.h"

static void uci_position_command(const Game *g, str_t *cmd)
// Builds a string of the form "position fen ... [moves ...]". Implements rule50 pruning: start from
// the last position that reset the rule50 counter, to reduce the move list to the minimum, without
// losing information.
{
    // Index of the starting FEN, where rule50 was last reset
    const int ply0 = max(g->ply - g->pos[g->ply].rule50, 0);

    scope(str_destroy) str_t fen = str_init();
    pos_get(&g->pos[ply0], &fen, g->sfen);
    str_cpy_fmt(cmd, "position fen %S", fen);

    if (ply0 < g->ply) {
        scope(str_destroy) str_t lan = str_init();
        str_cat_c(cmd, " moves");

        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            pos_move_to_lan(&g->pos[ply - 1], g->pos[ply].lastMove, &lan);
            str_cat(str_push(cmd, ' '), lan);
        }
    }
}


static void xboard_position_command(const Game *g, str_t *cmd)
// Builds a string of the form "setboard fen \n [<move \n>...]". Implements rule50 pruning: start from
// the last position that reset the rule50 counter, to reduce the move list to the minimum, without
// losing information.
{
    // Index of the starting FEN, where rule50 was last reset
    const int ply0 = max(g->ply - g->pos[g->ply].rule50, 0);

    scope(str_destroy) str_t fen = str_init();
    pos_get(&g->pos[ply0], &fen, g->sfen);
    str_cpy_fmt(cmd, "setboard %S\nforce\n", fen);

    if (ply0 < g->ply) {
        scope(str_destroy) str_t lan = str_init();
        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            pos_move_to_lan(&g->pos[ply - 1], g->pos[ply].lastMove, &lan);
            str_cat(str_push(cmd, '\n'), lan);
        }
    }
}

// Send position update to xboard after one move
static void xboard_update_position_command(const Game *g, str_t *cmd)
{
    str_cpy_fmt(cmd, "force\n");
    scope(str_destroy) str_t lan = str_init();
    pos_move_to_lan(&g->pos[g->ply - 1], g->pos[g->ply].lastMove, &lan);
    str_cat(cmd, lan);
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

    if (eo[ei]->time || eo[ei]->increment) {
        const int color = g->pos[g->ply].turn;

        str_cat_fmt(cmd, " wtime %I winc %I btime %I binc %I",
            timeLeft[ei ^ color], eo[ei ^ color]->increment,
            timeLeft[ei ^ color ^ BLACK], eo[ei ^ color ^ BLACK]->increment);
    }

    if (eo[ei]->movestogo)
        str_cat_fmt(cmd, " movestogo %i",
            eo[ei]->movestogo - ((g->ply / 2) % eo[ei]->movestogo));
}

static void xboard_go_command(Game *g, const EngineOptions *eo[2], int ei, const int64_t timeLeft[2],
    str_t *cmd)
{
    str_cpy_c(cmd, "");
    if (eo[ei]->time) {
        str_cat_fmt(cmd, "time %I\notime %I\n",
            timeLeft[ei] / 10,
            timeLeft[1-ei] / 10);
    }
    str_cat_fmt(cmd, "go");
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
    scope(str_destroy) str_t token = str_init();
    const char *tail = pv;

    // Start with current position. We can't guarantee that the resolved position won't be in check,
    // but a valid one must be returned.
    Position resolved = g->pos[g->ply];

    Position p[2];
    p[0] = resolved;
    int idx = 0;
    move_t *moves = vec_init_reserve(64, move_t);

    while ((tail = str_tok(tail, &token, " "))) {
        const move_t m = pos_lan_to_move(&p[idx], token.buf);
        moves = gen_all_moves(&p[idx], moves);

        if (illegal_move(m, moves)) {
            printf("[%d] WARNING: Illegal move in PV '%s%s' from %s\n", w->id, token.buf, tail,
                g->names[g->pos[g->ply].turn].buf);

            if (w->log)
                DIE_IF(w->id, fprintf(w->log, "WARNING: illegal move in PV '%s%s'\n", token.buf,
                    tail) < 0);

            break;
        }

        pos_move(&p[(idx + 1) % 2], &p[idx], m);
        idx = (idx + 1) % 2;

        if (!p[idx].checkers)
            resolved = p[idx];
    }

    vec_destroy(moves);
    return resolved;
}

Game game_init(int round, int game)
{
    Game g = {0};

    g.round = round;
    g.game = game;

    g.names[WHITE] = str_init();
    g.names[BLACK] = str_init();

    g.pos = vec_init(Position);
    g.info = vec_init(Info);
    g.samples = vec_init(Sample);

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

void game_destroy(Game *g)
{
    vec_destroy(g->samples);
    vec_destroy(g->info);
    vec_destroy(g->pos);

    str_destroy_n(&g->names[WHITE], &g->names[BLACK]);
}

static void xboard_setup_game_time(Worker *w, const Engine *engine, const EngineOptions *eo)
{
    scope(str_destroy) str_t s = str_init();
	if (eo->movetime) {
		if (eo->movetime % 1000 != 0)
			DIE("xboard protocol only supports whole second move times\n");
    	str_cat_fmt(&s, "st %I", eo->movetime / 1000);
	} else {
		if (eo->increment % 1000 != 0)
			DIE("xboard protocol only supports whole second increments\n");
		int seconds = eo->time / 1000;
		int inc = eo->increment / 1000;
    	str_cat_fmt(&s, "level %i %i:%i %I", eo->movestogo, seconds / 60, seconds % 60, inc);
	}
	engine_writeln(w, engine, s.buf);
}


int game_play(Worker *w, Game *g, const Options *o, const Engine engines[2],
    const EngineOptions *eo[2], bool reverse)
// Play a game:
// - engines[reverse] plays the first move (which does not mean white, that depends on the FEN)
// - sets g->state value: see enum STATE_* codes
// - returns RESULT_LOSS/DRAW/WIN from engines[0] pov
{
    for (int color = WHITE; color <= BLACK; color++)
        str_cpy(&g->names[color], engines[color ^ g->pos[0].turn ^ reverse].name);

    for (int i = 0; i < 2; i++) {
        if (g->pos[0].chess960) {
            if (engines[i].supportChess960)
                engine_writeln(w, &engines[i], "setoption name UCI_Chess960 value true");
            else
                DIE("[%d] '%s' does not support Chess960\n", w->id, engines[i].name.buf);
        }

        if (eo[i]->proto == PROTO_UCI) {
            engine_writeln(w, &engines[i], "ucinewgame");
        } else {
            engine_writeln(w, &engines[i], "new");
            xboard_setup_game_time(w, &engines[i], eo[i]);
        }
        engine_sync(w, &engines[i]);
    }

    scope(str_destroy) str_t cmd = str_init(), best = str_init();
    move_t played = 0;
    int drawPlyCount = 0;
    int resignCount[NB_COLOR] = {0};
    int ei = reverse;  // engines[ei] has the move
    int64_t timeLeft[2] = {eo[0]->time, eo[1]->time};
    scope(str_destroy) str_t pv = str_init();
    move_t *legalMoves = vec_init_reserve(64, move_t);

    for (g->ply = 0; ; ei = 1 - ei, g->ply++) {
        if (played)
            pos_move(&g->pos[g->ply], &g->pos[g->ply - 1], played);

        if ((g->state = game_apply_chess_rules(g, &legalMoves)))
            break;

        if (eo[ei]->proto == PROTO_UCI) {
                uci_position_command(g, &cmd);
        } else {
            if (g->ply < 2)
                xboard_position_command(g, &cmd);
            else
                xboard_update_position_command(g, &cmd);
        }
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
            timeLeft[ei] = INT64_MAX / 2;  // HACK: system_msec() + timeLeft must not overflow

        if (eo[ei]->proto == PROTO_UCI) {
            uci_go_command(g, eo, ei, timeLeft, &cmd);
        } else {
            xboard_go_command(g, eo, ei, timeLeft, &cmd);
        }

        engine_writeln(w, &engines[ei], cmd.buf);

        Info info = {0};
        const bool ok = engine_bestmove(w, &engines[ei], &timeLeft[ei], &best, &pv, &info);
        vec_push(g->info, info);

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

        if ((eo[ei]->time || eo[ei]->increment || eo[ei]->movetime) && timeLeft[ei] < 0) {
            g->state = STATE_TIME_LOSS;
            break;
        }

        // Apply draw adjudication rule
        if (o->drawCount && abs(info.score) <= o->drawScore) {
            if (++drawPlyCount >= 2 * o->drawCount) {
                g->state = STATE_DRAW_ADJUDICATION;
                break;
            }
        } else
            drawPlyCount = 0;

        // Apply resign rule
        if (o->resignCount && info.score <= -o->resignScore) {
            if (++resignCount[ei] >= o->resignCount) {
                g->state = STATE_RESIGN;
                break;
            }
        } else
            resignCount[ei] = 0;

        // Write sample: position (compactly encoded) + score
        if (prngf(&w->seed) <= o->sampleFrequency) {
            Sample sample = {
                .pos = o->sampleResolvePv ? resolved : g->pos[g->ply],
                .score = info.score,
                .result = NB_RESULT // unknown yet (use invalid state for now)
            };

            // Record sample, except if resolvePv=true and the position is in check (becuase PV
            // resolution couldn't avoid it), in which case the sample is discarded.
            if (!o->sampleResolvePv || !sample.pos.checkers)
                vec_push(g->samples, sample);
        }

        vec_push(g->pos, (Position){0});
    }

    assert(g->state != STATE_NONE);
    vec_destroy(legalMoves);

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

void game_export_pgn(const Game *g, int verbosity, str_t *out)
{
    str_cpy_fmt(out, "[Round \"%i.%i\"]\n", g->round + 1, g->game + 1);
    str_cat_fmt(out, "[White \"%S\"]\n", g->names[WHITE]);
    str_cat_fmt(out, "[Black \"%S\"]\n", g->names[BLACK]);

    // Result in PGN format "1-0", "0-1", "1/2-1/2" (from white pov)
    scope(str_destroy) str_t result = str_init(), reason = str_init();
    game_decode_state(g, &result, &reason);
    str_cat_fmt(out, "[Result \"%S\"]\n", result);
    str_cat_fmt(out, "[Termination \"%S\"]\n", reason);

    scope(str_destroy) str_t fen = str_init();
    pos_get(&g->pos[0], &fen, g->sfen);
    str_cat_fmt(out, "[FEN \"%S\"]\n", fen);

    if (g->pos[0].chess960)
        str_cat_c(out, "[Variant \"Chess960\"]\n");

    str_cat_fmt(out, "[PlyCount \"%i\"]\n", g->ply);
    scope(str_destroy) str_t san = str_init();

    if (verbosity > 0) {
        // Print the moves
        str_push(out, '\n');

        const int pliesPerLine = verbosity == 2 ? 6
            : verbosity == 3 ? 5
            : 16;

        for (int ply = 1; ply <= g->ply; ply++) {
            // Write move number
            if (g->pos[ply - 1].turn == WHITE || ply == 1)
                str_cat_fmt(out, g->pos[ply - 1].turn == WHITE ? "%i. " : "%i... ",
                    g->pos[ply - 1].fullMove);

            // Append SAN move
            pos_move_to_san(&g->pos[ply - 1], g->pos[ply].lastMove, &san);
            str_cat(out, san);

            // Append check marker
            if (g->pos[ply].checkers) {
                if (ply == g->ply && g->state == STATE_CHECKMATE)
                    str_push(out, '#');  // checkmate
                else
                    str_push(out, '+');  // normal check
            }

            // Write PGN comment
            const int depth = g->info[ply - 1].depth, score = g->info[ply - 1].score;

            if (verbosity == 2) {
                if (score > INT_MAX / 2)
                    str_cat_fmt(out, " {M%i/%i}", INT_MAX - score, depth);
                else if (score < INT_MIN / 2)
                    str_cat_fmt(out, " {-M%i/%i}", score - INT_MIN, depth);
                else
                    str_cat_fmt(out, " {%i/%i}", score, depth);
            } else if (verbosity == 3) {
                const int64_t time = g->info[ply - 1].time;

                if (score > INT_MAX / 2)
                    str_cat_fmt(out, " {M%i/%i %Ims}", INT_MAX - score, depth, time);
                else if (score < INT_MIN / 2)
                    str_cat_fmt(out, " {-M%i/%i %Ims}", score - INT_MIN, depth, time);
                else
                    str_cat_fmt(out, " {%i/%i %Ims}", score, depth, time);
            }

            // Append delimiter
            str_push(out, ply % pliesPerLine == 0 ? '\n' : ' ');
        }
    }

    str_cat_c(str_cat(out, result), "\n\n");
}

void game_export_samples(const Game *g, str_t *out)
{
    str_clear(out);
    scope(str_destroy) str_t fen = str_init();

    for (size_t i = 0; i < vec_size(g->samples); i++) {
        pos_get(&g->samples[i].pos, &fen, g->sfen);
        str_cat_fmt(out, "%S,%i,%i\n", fen, g->samples[i].score, g->samples[i].result);
    }
}
