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
    str_free(&fen);

    if (ply0 < g->ply) {
        str_cat(&cmd, " moves");

        for (int ply = ply0 + 1; ply <= g->ply; ply++) {
            str_t lan = pos_move_to_lan(&g->pos[ply - 1], g->pos[ply].lastMove, g->chess960);
            str_cat(&cmd, " ", lan.buf);
            str_free(&lan);
        }
    }

    str_cat(&cmd, "\n");
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
        // Scan for 3-fold repetition
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

void game_create(Game *g, bool chess960, const char *fen)
{
    g->chess960 = chess960;
    g->ply = 0;
    g->result = RESULT_NONE;
    pos_set(&g->pos[0], fen);

    g->names[WHITE] = str_new();
    g->names[BLACK] = str_new();
}

void game_destroy(Game *g)
{
    str_free(&g->names[WHITE], &g->names[BLACK]);
}

void game_play(Game *g, const Engine *first, const Engine *second)
{
    const Engine *engines[2] = {first, second};  // more practical for loops

    for (int color = WHITE; color <= BLACK; color++)
        str_cpy(&g->names[color], engines[color ^ g->pos[0].turn]->name.buf);

    for (int i = 0; i < 2; i++) {
        engine_writeln(engines[i], "ucinewgame\n");
        engine_sync(engines[i]);
    }

    move_t move = 0;

    for (g->ply = 0; g->ply < MAX_GAME_PLY; g->ply++) {
        if (g->ply > 0)
            pos_move(&g->pos[g->ply], &g->pos[g->ply - 1], move);

        move_t moves[MAX_MOVES], *end;

        if ((g->result = game_result(g, moves, &end)))
            break;

        const Engine *engine = engines[g->ply % 2];  // engine whose turn it is to play

        str_t posCmd = uci_position_command(g);
        engine_writeln(engine, posCmd.buf);
        str_free(&posCmd);

        engine_sync(engine);

        engine_writeln(engine, "go depth 6\n");
        str_t played = engine_bestmove(engine);
        move = pos_string_to_move(&g->pos[g->ply], played.buf, g->chess960);
        str_free(&played);

        if (illegal_move(move, moves, end)) {
            g->result = RESULT_ILLEGAL_MOVE;
            break;
        }
    }

    if (!g->result) {
        assert(g->ply == MAX_GAME_PLY);
        g->result = RESULT_MAX_PLY;
    }
}

str_t game_pgn_result(int result, int lastTurn, str_t *pgnTermination)
{
    str_t pgnResult = str_new();
    str_cpy(pgnTermination, "normal");  // default: termination by chess rules

    if (result == RESULT_NONE) {
        str_cpy(&pgnResult, "*");
        str_cpy(pgnTermination, "unterminated");
    } else if (result == RESULT_CHECKMATE)
        str_cpy(&pgnResult, lastTurn == WHITE ? "0-1" : "1-0");
    else if (result == RESULT_STALEMATE)
        str_cpy(&pgnResult, "1/2-1/2");
    else if (result == RESULT_THREEFOLD)
        str_cpy(&pgnResult, "1/2-1/2");
    else if (result == RESULT_FIFTY_MOVES)
        str_cpy(&pgnResult, "1/2-1/2");
    else if (result ==RESULT_INSUFFICIENT_MATERIAL)
        str_cpy(&pgnResult, "1/2-1/2");
    else if (result == RESULT_ILLEGAL_MOVE) {
        str_cpy(&pgnResult, lastTurn == WHITE ? "0-1" : "1-0");
        str_cpy(pgnTermination, "illegal move");
    } else if (result == RESULT_MAX_PLY) {
        str_cpy(&pgnResult, "1/2-1/2");
        str_cpy(pgnTermination, "max ply");
    }

    return pgnResult;
}

str_t game_pgn(const Game *g)
// FIXME: Use SAN as required by PGN
{
    str_t pgn = str_new();

    str_cat(&pgn, "[White \"", g->names[WHITE].buf, "\"]\n");
    str_cat(&pgn, "[Black \"", g->names[BLACK].buf, "\"]\n");

    str_t fen = pos_get(&g->pos[0]);
    str_cat(&pgn, "[FEN \"", fen.buf, "\"]\n");
    str_free(&fen);

    if (g->chess960)
        str_cat(&pgn, "[Variant \"Chess960\"]\n");

    // Result in PGN format "1-0", "0-1", "1/2-1/2" (from white pov)
    str_t pgnTermination = str_new();
    str_t pgnResult = game_pgn_result(g->result, g->pos[g->ply].turn, &pgnTermination);
    str_cat(&pgn, "[Result \"", pgnResult.buf, "\"]\n");
    str_cat(&pgn, "[Termination \"", pgnTermination.buf, "\"]\n\n");

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
        str_free(&san);
    }

    str_cat(&pgn, pgnResult.buf, "\n\n");
    str_free(&pgnResult, &pgnTermination);
    return pgn;
}
