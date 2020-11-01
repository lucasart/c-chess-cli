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
#include "jobs.h"
#include "openings.h"
#include "options.h"
#include "sprt.h"
#include "util.h"
#include "vec.h"
#include "workers.h"

static Options options;
static EngineOptions *eo;
static Openings openings;
static FILE *pgn, *sampleFile;
static JobQueue jq;

static void main_init(int argc, const char **argv)
{
    eo = vec_init(EngineOptions);
    options = options_init();
    options_parse(argc, argv, &options, &eo);

    jq = job_queue_init(vec_size(eo), options.rounds, options.games, options.gauntlet);
    openings = openings_init(options.openings.buf, options.random, 0);

    pgn = NULL;

    if (options.pgn.len)
        DIE_IF(0, !(pgn = fopen(options.pgn.buf, "a")));

    sampleFile = NULL;

    if (options.sampleFileName.len)
        DIE_IF(0, !(sampleFile = fopen(options.sampleFileName.buf, "a")));

    // Prepare Workers[]
    Workers = vec_init(Worker);

    for (int i = 0; i < options.concurrency; i++) {
        scope(str_destroy) str_t logName = str_init();

        if (options.log)
            str_cat_fmt(&logName, "c-chess-cli.%i.log", i + 1);

        vec_push(Workers, worker_init(i, logName.buf));
    }
}

static void main_destroy(void)
{
    vec_destroy_rec(Workers, worker_destroy);

    if (pgn)
        DIE_IF(0, fclose(pgn) < 0);

    if (sampleFile)
        DIE_IF(0, fclose(sampleFile) < 0);

    openings_destroy(&openings, 0);
    job_queue_destroy(&jq);
    options_destroy(&options);
    vec_destroy_rec(eo, engine_options_destroy);
}

static void *thread_start(void *arg)
{
    Worker *w = arg;
    Engine engines[2] = {0};

    scope(str_destroy) str_t fen = str_init();
    Job j = {0};
    int e1 = 0, e2 = 1;  // eo[e1] plays eo[e2]
    size_t idx = 0, count = 0;  // game idx and count (shared across workers)

    // Prepare engines[]
    engines[0] = engine_init(w, eo[e1].cmd.buf, eo[e1].name.buf, eo[e1].options);
    engines[1] = engine_init(w, eo[e2].cmd.buf, eo[e2].name.buf, eo[e2].options);

    while (job_queue_pop(&jq, &j, &idx, &count)) {
        // Engine switching (if needed)
        if (j.e1 != e1) {
            e1 = j.e1;
            engine_destroy(w, &engines[0]);
            engines[0] = engine_init(w, eo[e1].cmd.buf, eo[e1].name.buf, eo[e1].options);
        }

        if (j.e2 != e2) {
            e2 = j.e2;
            engine_destroy(w, &engines[1]);
            engines[1] = engine_init(w, eo[e2].cmd.buf, eo[e2].name.buf, eo[e2].options);
        }

        // Choose opening position
        openings_next(&openings, &fen, options.repeat ? idx / 2 : idx, w->id);

        // Play 1 game
        Game game = game_init(j.round, j.game);
        int color = WHITE;

        if (!game_load_fen(&game, fen.buf, &color))
            DIE("[%d] illegal FEN '%s'\n", w->id, fen.buf);

        const int whiteIdx = color ^ j.reverse;

        printf("[%d] Started game %zu of %zu (%s vs %s)\n", w->id, idx + 1, count,
            engines[whiteIdx].name.buf, engines[opposite(whiteIdx)].name.buf);

        const EngineOptions *eoPair[2] = {&eo[e1], &eo[e2]};
        const int wld = game_play(w, &game, &options, engines, eoPair, j.reverse);

        // Write to PGN file
        if (pgn) {
            scope(str_destroy) str_t pgnText = str_init();
            game_pgn(&game, &pgnText);
            DIE_IF(w->id, fputs(pgnText.buf, pgn) < 0);
        }

        // Write to Sample file
        if (sampleFile) {
            scope(str_destroy) str_t lines = str_init(), sampleFen = str_init();

            for (size_t i = 0; i < vec_size(game.samples); i++) {
                pos_get(&game.samples[i].pos, &sampleFen, game.sfen);
                str_cat_fmt(&lines, "%S,%i,%i\n", sampleFen, game.samples[i].score,
                    game.samples[i].result);
            }

            DIE_IF(w->id, fputs(lines.buf, sampleFile) < 0);
        }

        // Write to stdout a one line summary of the game
        scope(str_destroy) str_t result = str_init(), reason = str_init();
        game_decode_state(&game, &result, &reason);

        printf("[%d] Finished game %zu (%s vs %s): %s {%s}\n", w->id, idx + 1,
            engines[whiteIdx].name.buf, engines[opposite(whiteIdx)].name.buf, result.buf, reason.buf);

        // Update on global score (across workers)
        int wldCount[3] = {0};
        job_queue_add_result(&jq, j.pair, wld, wldCount);

        const int n = wldCount[RESULT_WIN] + wldCount[RESULT_LOSS] + wldCount[RESULT_DRAW];

        printf("Score of %s vs %s: %d - %d - %d  [%.3f] %d\n", engines[0].name.buf,
            engines[1].name.buf, wldCount[RESULT_WIN], wldCount[RESULT_LOSS], wldCount[RESULT_DRAW],
            (wldCount[RESULT_WIN] + 0.5 * wldCount[RESULT_DRAW]) / n, n);

        if (options.sprt && sprt_done(wldCount, &options.sprtParam))
            job_queue_stop(&jq);

        game_destroy(&game);
    }

    for (int i = 0; i < 2; i++)
        engine_destroy(w, &engines[i]);

    return NULL;
}

int main(int argc, const char **argv)
{
    main_init(argc, argv);

    // Start threads[]
    pthread_t threads[options.concurrency];

    for (int i = 0; i < options.concurrency; i++)
        pthread_create(&threads[i], NULL, thread_start, &Workers[i]);

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
    } while (!job_queue_done(&jq));

    // Join threads[]
    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    main_destroy();
    return 0;
}
