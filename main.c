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
#include <pthread.h>
#include <stdlib.h>
#include "engine.h"
#include "game.h"
#include "openings.h"
#include "options.h"
#include "workers.h"

Options options;
Openings openings;
FILE *pgnout;

static double score_pct(int wld[3])
{
    return 100 * (wld[RESULT_WIN] + 0.5 * wld[RESULT_DRAW]) / (wld[RESULT_WIN] + wld[RESULT_LOSS]
        + wld[RESULT_DRAW]);
}

static void *thread_start(void *arg)
{
    Worker *worker = arg;
    Engine engines[2];

    str_t logName = str_new();
    str_cat_fmt(&logName, "c-chess-cli.%i.log", worker->id);
    FILE *log = options.debug ? fopen(logName.buf, "w") : NULL;
    str_delete(&logName);

    for (int i = 0; i < 2; i++)
        engines[i] = engine_create(options.cmd[i].buf, log, options.uciOptions[i].buf);

    int next;
    str_t fen = str_new();

    while ((next = openings_next(&openings, &fen)) <= options.games) {
        // Play 1 game
        Game game = game_new(fen.buf, &options.go);
        const int wld = game_play(&game, engines, options.repeat && !(next % 2));

        // Write to stdout a one line summary
        str_t reason = str_new();
        str_t result = game_decode_state(&game, &reason);
        printf("[%i] %s vs. %s: %s (%s)\n", worker->id, game.names[WHITE].buf,
            game.names[BLACK].buf, result.buf, reason.buf);
        str_delete(&result, &reason);

        // Write to PGN file
        if (pgnout) {
            str_t pgn = game_pgn(&game);
            fputs(pgn.buf, pgnout);
            str_delete(&pgn);
        }

        int wldCount[3];
        workers_add_result(worker, wld, wldCount);

        printf("[%d] score of %s vs. %s: %d-%d-%d [%.2f%%]\n", worker->id, engines[0].name.buf,
            engines[1].name.buf, wldCount[RESULT_WIN], wldCount[RESULT_LOSS], wldCount[RESULT_DRAW],
            score_pct(wldCount));

        game_delete(&game);
    }

    str_delete(&fen);

    for (int i = 0; i < 2; i++)
        engine_delete(&engines[i]);

    if (log)
        fclose(log);

    return NULL;
}

int main(int argc, const char **argv)
{
    options = options_new(argc, argv);
    openings = openings_new(options.openings.buf, options.random, options.repeat);
    pgnout = options.pgnout.len ? fopen(options.pgnout.buf, "w") : NULL;

    pthread_t threads[options.concurrency];

    workers_new(options.concurrency);

    for (int i = 0; i < options.concurrency; i++)
        pthread_create(&threads[i], NULL, thread_start, &Workers[i]);

    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    workers_delete(Workers);

    if (pgnout)
        fclose(pgnout);

    openings_delete(&openings);
    options_delete(&options);
    return 0;
}
