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
#include <string.h>

int main(int argc, char **argv)
{
    if (argc == 3) {
        Game game;
        game.chess960 = false;
        game.result = 0;
        pos_set(&game.pos[0], "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");

        // Prepare engines
        for (int i = 0; i < 2; i++) {
            strcpy(game.engines[i].name, argv[i + 1]);
            engine_start(&game.engines[i], argv[i + 1]);

            char line[1024];
            engine_writeln(&game.engines[i], "uci\n");

            while (engine_readln(&game.engines[i], line, sizeof line)
                && strcmp(line, "uciok\n"));
        }

        // Play a game
        play_game(&game);

        // Close engines
        for (int i = 0; i < 2; i++) {
            engine_writeln(&game.engines[i], "quit\n");
            engine_stop(&game.engines[i]);
        }

        game_print(&game, stdout);
    } else
        gen_run_test();
}
