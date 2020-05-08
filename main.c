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
#include "str.h"
#include "openings.h"

enum {N_GAMES = 10};

int main(int argc, char **argv)
{
    if (argc == 4) {
        Engine engines[2];

        // Prepare engines
        for (int i = 0; i < 2; i++)
            engine_create(&engines[i], argv[i + 1], /*stderr*/ NULL);

        Openings o;
        openings_create(&o, argv[3], true);

        for (int i = 0; i < N_GAMES; i++) {
            // Load opening and prepare game
            Game game;
            str_t fen = openings_get(&o);
            game_create(&game, false, fen.buf);

            // Play the game
            game_play(&game, &engines[0], &engines[1]);

            // Print the PGN
            str_t pgn = game_pgn(&game);
            puts(pgn.buf);

            // Clean-up
            str_free(&fen, &pgn);
            game_destroy(&game);
        }

        openings_destroy(&o);

        // Kill engines
        for (int i = 0; i < 2; i++)
            engine_destroy(&engines[i]);
    } else
        gen_run_test();
}
