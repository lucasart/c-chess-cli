#include "game.h"

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

void play_game(Game *game)
{
    for (int i = 0; i < 2; i++) {
        engine_writeln(&game->e[i], "ucinewgame\n");
        engine_sync(&game->e[i]);
    }

    for (game->ply = 0; game->ply < MAX_GAME_PLY; game->ply++) {
        const Engine *eng = &game->e[game->ply % 2];  // engine whose turn it is
        const Position *before = &game->pos[game->ply];  // before playing the move
        Position *after = &game->pos[game->ply + 1];  // after playing the move

        char str[MAX_POSITION_CHAR];
        build_position_command(game, str);

        engine_writeln(eng, str);
        engine_sync(eng);

        char moveStr[MAX_MOVE_CHAR];
        engine_writeln(eng, "go depth 4\n");
        engine_bestmove(eng, moveStr);

        const move_t move = pos_string_to_move(before, moveStr, game->chess960);
        pos_move(after, before, move);
        pos_print(after);
    }
}
