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
#include "sprt.h"
#include "util.h"
#include "workers.h"

static Options options;
static EngineOptions *eo;
static GameOptions go;
static Openings openings;
static FILE *pgnOut, *sampleFile;

static void *thread_start(void *arg)
{
    Worker *w = arg;
    Engine engines[2];

    // Prepare engines[]
    for (int i = 0; i < 2; i++)
        engines[i] = engine_new(w, eo[i].cmd.buf, eo[i].name.buf, eo[i].options);

    int next;
    scope(str_del) str_t fen = str_new();

    while ((next = openings_next(&openings, &fen, w->id)) <= options.games) {
        // Play 1 game
        Game game = game_new(w, fen.buf);
        const EngineOptions *eoPair[2] = {&eo[0], &eo[1]};
        const int wld = game_play(w, &game, &go, engines, eoPair, next % 2 == 0);

        // Write to PGN file
        if (pgnOut) {
            scope(str_del) str_t pgn = str_new();
            game_pgn(&game, &pgn);
            DIE_IF(w->id, fputs(pgn.buf, pgnOut) < 0);
        }

        // Write to Sample file
        if (sampleFile) {
            scope(str_del) str_t lines = str_new(), sampleFen = str_new();

            for (size_t i = 0; i < vec_size(game.samples); i++) {
                pos_get(&game.samples[i].pos, &sampleFen);
                str_cat_fmt(&lines, "%S,%i,%i\n", sampleFen, game.samples[i].score,
                    game.samples[i].result);
            }

            DIE_IF(w->id, fputs(lines.buf, sampleFile) < 0);
        }

        // Write to stdout a one line summary of the game
        scope(str_del) str_t result = str_new(), reason = str_new();
        game_decode_state(&game, &result, &reason);
        printf("[%i] %s vs %s: %s (%s)\n", w->id, game.names[WHITE].buf,
            game.names[BLACK].buf, result.buf, reason.buf);

        // Update on global score (across workers)
        int wldCount[3];
        workers_add_result(w, wld, wldCount);

        const int n = wldCount[RESULT_WIN] + wldCount[RESULT_LOSS] + wldCount[RESULT_DRAW];

        printf("Score of %s vs %s: %d - %d - %d  [%.3f] %d\n", engines[0].name.buf,
            engines[1].name.buf, wldCount[RESULT_WIN], wldCount[RESULT_LOSS], wldCount[RESULT_DRAW],
            (wldCount[RESULT_WIN] + 0.5 * wldCount[RESULT_DRAW]) / n, n);

        if (options.sprt) {
            double llrLbound, llrUbound;
            sprt_bounds(options.alpha, options.beta, &llrLbound, &llrUbound);

            const double llr = sprt_llr(wldCount, options.elo0, options.elo1);

            if (llr > llrUbound)
                DIE("SPRT: LLR = %.3f [%.3f,%.3f]. H1 accepted.\n", llr, llrLbound, llrUbound);

            if (llr < llrLbound)
                DIE("SPRT: LLR = %.3f [%.3f,%.3f]. H0 accepted.\n", llr, llrLbound, llrUbound);

            if (next % 2 == 0)
                printf("SPRT: LLR = %.3f [%.3f,%.3f]\n", llr, llrLbound, llrUbound);
        }

        game_del(&game);
    }

    for (int i = 0; i < 2; i++)
        engine_del(w, &engines[i]);

    WorkersBusy--;
    return NULL;
}

int main(int argc, const char **argv)
{
    eo = vec_new(2, EngineOptions);
    options = options_new();
    options_parse(argc, argv, &options, &go, &eo);

    openings = openings_new(options.openings.buf, options.random, options.repeat, 0);

    pgnOut = NULL;
    if (options.pgnOut.len)
        DIE_IF(0, !(pgnOut = fopen(options.pgnOut.buf, "a")));

    sampleFile = NULL;
    if (options.sampleFileName.len)
        DIE_IF(0, !(sampleFile = fopen(options.sampleFileName.buf, "a")));

    // Prepare Workers[]
    Workers = vec_new((size_t)options.concurrency, Worker);

    for (int i = 0; i < options.concurrency; i++) {
        scope(str_del) str_t logName = str_new();

        if (options.log)
            str_cat_fmt(&logName, "c-chess-cli.%i.log", i + 1);

        vec_push(Workers, worker_new(i, logName.buf));
    }

    // Start threads[]
    pthread_t threads[options.concurrency];

    for (int i = 0; i < options.concurrency; i++) {
        WorkersBusy++;
        pthread_create(&threads[i], NULL, thread_start, &Workers[i]);
    }

    // Main thread loop: check deadline overdue at regular intervals
    do {
        system_sleep(100);

        for (int i = 0; i < options.concurrency; i++) {
            const int64_t delay = deadline_overdue(&Workers[i]);

            // We want some tolerance on small delays here. Given a choice, it's best to wait for
            // the worker thread to notice an overdue deadline, which it will handled nicely by
            // counting the game as lost for the offending engine, and continue. Enforcing deadlines
            // from the master thread is the last resort solution, because it is an unrecovrable
            // error. At this point we are likely to face a completely unresponsive engine, where
            // any attempt at I/O will block the master thread, on top of the already blocked
            // worker. Hence, we must DIE().
            if (delay > 1000)
                DIE("[%d] engine %s is unresponsive\n", i, Workers[i].deadline.engineName.buf);
        }
    } while (WorkersBusy > 0);

    // Join threads[]
    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    // Cleanup
    vec_del_rec(Workers, worker_del);

    if (pgnOut)
        DIE_IF(0, fclose(pgnOut) < 0);

    if (sampleFile)
        DIE_IF(0, fclose(sampleFile) < 0);

    openings_delete(&openings, 0);
    options_del(&options);
    vec_del_rec(eo, engine_options_del);

    return 0;
}
