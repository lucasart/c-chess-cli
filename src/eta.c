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
#include "eta.h"
#include "util.h"
#include <time.h>
#include <string.h>

int print_eta(Options options, EngineOptions *vecEO) {
    if (!vecEO)
        return 1;

    time_t eta = time(NULL);
    
    int average_num_moves = 46;
    float time_usage = 0.94;

    time_t time_per_game;
    time_t time_per_game_time = 2 * (vecEO->time + average_num_moves * vecEO->increment);
    time_t time_per_game_movetime = 2 * average_num_moves * vecEO->movetime;
    if (!vecEO->movetime)
	    time_per_game = time_per_game_time;
    else if (!vecEO->time)
	    time_per_game = time_per_game_movetime;
    else
	    time_per_game = min(time_per_game_time, time_per_game_movetime);

    if (options.concurrency)
        eta += (time_t)(time_usage * time_per_game * options.games * options.rounds / (1000 * options.concurrency));

    printf("ETA: %s", ctime(&eta));
    return 0;
}
