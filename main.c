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

int main(int argc, char **argv)
{
    if (argc == 3) {
        Engine engines[2];

        // Prepare engines
        for (int i = 0; i < 2; i++)
            engine_create(&engines[i], argv[i + 1], stderr);

        // Play and print a game
        Game game;
        game_create(&game, false, "rnbqkbnr/ppp1ppp1/8/3p3p/8/2P1P3/PPQP1PPP/RNB1KBNR b KQkq - 1 3");
        game_play(&game, &engines[0], &engines[1]);
        game_print(&game, stdout);
        game_destroy(&game);

        // Kill engines
        for (int i = 0; i < 2; i++)
            engine_destroy(&engines[i]);
    } else
        gen_run_test();
}
