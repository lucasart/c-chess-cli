#include "game.h"
#include "gen.h"

void build_position_command(const Game *game, char *str)
// Builds a string of the form position fen ... [moves ...]. Implements rule50 pruning, which means
// start from the last position that reset the rule50 counter. This produces the shortest string,
// without any loss of information.
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
    GenInfo gi;
    const Position *pos = &game->pos[game->ply];

    *end = gen_all_moves(pos, &gi, begin);

    if (*end == begin)
        return gi.checkers ? RESULT_MATE : RESULT_STALEMATE;
    else if (pos->rule50 >= 100) {
        assert(pos->rule50 == 100);
        return RESULT_FIFTY_MOVES;
    } else if (pos_insufficient_material(pos))
        return RESULT_INSUFFICIENT_MATERIAL;

    // TODO: 3 move repetition

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
        engine_writeln(engine, "go depth 4\n");
        engine_bestmove(engine, moveStr);

        move = pos_string_to_move(&game->pos[game->ply], moveStr, game->chess960);

        if (illegal_move(move, moves, end)) {
            game->result = RESULT_ILLEGAL_MOVE;
            break;
        }
    }

    if (!game->result) {
        assert(game->ply == MAX_GAME_PLY - 1);
        game->result = RESULT_MAX_PLY;
    }
}

void game_print(const Game *game, FILE *out)
// FIXME: Eventually we want PGN format
{
    char fen[MAX_FEN_CHAR];
    pos_get(&game->pos[0], fen);

    fprintf(out, "Start FEN: %s\n", fen);
    fprintf(out, "Chess960: %d\n", game->chess960);
    fprintf(out, "First player: %s\n", game->engines[0].name);
    fprintf(out, "Second player: %s\n", game->engines[1].name);
    fprintf(out, "Result: %d\n", game->result);

    for (int ply = 1; ply <= game->ply; ply++) {
        char moveStr[MAX_MOVE_CHAR];
        pos_move_to_string(&game->pos[ply - 1], game->pos[ply].lastMove, moveStr, game->chess960);
        fprintf(out, ply % 20 == 0 || ply == game->ply ? "%s\n" : "%s ", moveStr);
    }
}
