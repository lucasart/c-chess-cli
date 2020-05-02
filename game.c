#include "game.h"
#include "gen.h"

static void build_position_command(const Game *game, char *str)
// Builds a string of the form "position fen ... [moves ...]". Implements rule50 pruning: start from
// the last position that reset the rule50 counter, to reduce the move list to the minimum, without
// losing information.
{
    // Index of the starting FEN, where rule50 was last reset
    const int ply0 = max(game->ply - game->pos[game->ply].rule50, 0);

    char fen[MAX_FEN_CHAR];
    pos_get(&game->pos[ply0], fen);
    strcat(strcpy(str, "position fen "), fen);

    if (ply0 < game->ply) {
        strcat(str, " moves");

        for (int ply = ply0 + 1; ply <= game->ply; ply++) {
            char token[MAX_MOVE_CHAR + 1] = " ";
            pos_move_to_string(&game->pos[ply - 1], game->pos[ply].lastMove, token + 1,
                game->chess960);
            strcat(str, token);
        }
    }

    strcat(str, "\n");
}

int game_result(const Game *game, move_t *begin, move_t **end)
{
    const Position *pos = &game->pos[game->ply];

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

        for (int i = 4; i <= pos->rule50 && i <= game->ply; i += 2)
            if (game->pos[game->ply - i].key == pos->key && ++repetitions >= 3)
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

void play_game(Game *game)
{
    for (int i = 0; i < 2; i++) {
        engine_writeln(&game->engines[i], "ucinewgame\n");
        engine_sync(&game->engines[i]);
    }

    move_t move = 0;

    for (game->ply = 0; game->ply < MAX_GAME_PLY; game->ply++) {
        if (game->ply > 0)
            pos_move(&game->pos[game->ply], &game->pos[game->ply - 1], move);

        move_t moves[MAX_MOVES], *end;

        if ((game->result = game_result(game, moves, &end)))
            break;

        const Engine *engine = &game->engines[game->ply % 2];  // engine whose turn it is to play

        char posCmd[MAX_POSITION_CHAR];
        build_position_command(game, posCmd);

        engine_writeln(engine, posCmd);
        engine_sync(engine);

        char moveStr[MAX_MOVE_CHAR];
        engine_writeln(engine, "go depth 6\n");
        engine_bestmove(engine, moveStr);

        move = pos_string_to_move(&game->pos[game->ply], moveStr, game->chess960);

        if (illegal_move(move, moves, end)) {
            game->result = RESULT_ILLEGAL_MOVE;
            break;
        }
    }

    if (!game->result) {
        assert(game->ply == MAX_GAME_PLY);
        game->result = RESULT_MAX_PLY;
    }
}

void game_result_string(int result, int lastTurn, char *resultStr, char *terminationStr)
{
    strcpy(terminationStr, "normal");  // default: termination by chess rules

    if (result == RESULT_NONE) {
        strcpy(resultStr, "*");
        strcpy(terminationStr, "unterminated");
    } else if (result == RESULT_CHECKMATE)
        strcpy(resultStr, lastTurn == WHITE ? "0-1" : "1-0");
    else if (result == RESULT_STALEMATE)
        strcpy(resultStr, "1/2-1/2");
    else if (result == RESULT_THREEFOLD)
        strcpy(resultStr, "1/2-1/2");
    else if (result == RESULT_FIFTY_MOVES)
        strcpy(resultStr, "1/2-1/2");
    else if (result ==RESULT_INSUFFICIENT_MATERIAL)
        strcpy(resultStr, "1/2-1/2");
    else if (result == RESULT_ILLEGAL_MOVE) {
        strcpy(resultStr, lastTurn == WHITE ? "0-1" : "1-0");
        strcpy(terminationStr, "illegal move");
    } else if (result == RESULT_MAX_PLY) {
        strcpy(resultStr, "1/2-1/2");
        strcpy(terminationStr, "max ply");
    }
}

void game_print(const Game *game, FILE *out)
// FIXME: Use SAN as required by PGN
{
    char fen[MAX_FEN_CHAR], resultStr[8], terminationStr[16];

    // Scan initial position
    pos_get(&game->pos[0], fen);
    bool blackFirst = game->pos[0].turn;

    // Decode game result into human readable information
    game_result_string(game->result, game->pos[game->ply].turn, resultStr, terminationStr);

    fprintf(out, "[White \"%s\"]\n", game->engines[blackFirst].name);
    fprintf(out, "[Black \"%s\"]\n", game->engines[!blackFirst].name);
    fprintf(out, "[FEN \"%s\"]\n", fen);

    if (game->chess960)
        fputs("[Variant \"Chess960\"]", out);

    fprintf(out, "[Result \"%s\"]\n", resultStr);
    fprintf(out, "[Termination \"%s\"]\n\n", terminationStr);

    for (int ply = 1; ply <= game->ply; ply++) {
        // Prepare SAN base
        char san[MAX_MOVE_CHAR];
        pos_move_to_san(&game->pos[ply - 1], game->pos[ply].lastMove, san);

        // Append check markers to SAN
        if (game->pos[ply].checkers) {
            if (ply == game->ply && game->result == RESULT_CHECKMATE)
                strcat(san, "#");  // checkmate
            else
                strcat(san, "+");  // normal check
        }

        if (game->pos[ply - 1].turn == WHITE || ply == 1)
            fprintf(out, game->pos[ply - 1].turn == WHITE ? "%d. " : "%d.. ",
                game->pos[ply - 1].fullMove);

        fprintf(out, ply % 10 == 0 ? "%s\n" : "%s ", san);
    }

    fprintf(out, "%s\n\n", resultStr);
}
