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
#include "engine.h"
#include "game.h"
#include "jobs.h"
#include "openings.h"
#include "options.h"
#include "seqwriter.h"
#include "sprt.h"
#include "util.h"
#include "vec.h"
#include "workers.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static Options options;
static EngineOptions *vecEO;
static Openings openings;
static SeqWriter pgnSeqWriter;
static FILE *sampleFile;
static JobQueue jq;

static void main_destroy(void) {
    vec_destroy_rec(vecWorkers, worker_destroy);

    if (options.sp.fileName.len)
        fclose(sampleFile);

    if (options.pgn.len)
        seq_writer_destroy(&pgnSeqWriter);

    openings_destroy(&openings);
    job_queue_destroy(&jq);
    options_destroy(&options);
    vec_destroy_rec(vecEO, engine_options_destroy);
}

static void main_init(int argc, const char **argv) {
    atexit(main_destroy);

    options = options_init();
    vecEO = options_parse(argc, argv, &options);

    jq = job_queue_init((int)vec_size(vecEO), options.rounds, options.games, options.gauntlet);
    openings = openings_init(options.openings.buf, options.random, options.srand);

    if (options.pgn.len)
        pgnSeqWriter = seq_writer_init(options.pgn.buf, "a" FOPEN_TEXT);

    if (options.sp.fileName.len) {
        if (options.sp.bin)
            DIE_IF(!(sampleFile = fopen(options.sp.fileName.buf, "a" FOPEN_BINARY)));
        else
            DIE_IF(!(sampleFile = fopen(options.sp.fileName.buf, "a" FOPEN_TEXT)));
    }

    // Prepare vecWorkers[]
    vecWorkers = vec_init(Worker);

    for (int i = 0; i < options.concurrency; i++) {
        scope(str_destroy) str_t logName = str_init();

        if (options.log)
            str_cat_fmt(&logName, "c-chess-cli.%i.log", i + 1);

        vec_push(vecWorkers, worker_init(i, logName.buf));
    }
}

static void *thread_start(void *arg) {
    Worker *w = arg;
    threadId = w->id;
    Engine engines[2] = {0};

    scope(str_destroy) str_t fen = str_init();
    Job job = {0};
    int ei[2] = {-1,
                 -1}; // vecEO[ei[0]] plays vecEO[ei[1]]: initialize with invalid values to start
    size_t idx = 0, count = 0; // game idx and count (shared across vecWorkers)

    while (job_queue_pop(&jq, &job, &idx, &count)) {
        // Engine stop/start, as needed
        for (int i = 0; i < 2; i++)
            if (job.ei[i] != ei[i]) {
                ei[i] = job.ei[i];

                if (engines[i].in)
                    engine_destroy(w, &engines[i]);

                engines[i] = engine_init(w, vecEO[ei[i]].cmd.buf, vecEO[ei[i]].name.buf,
                                         vecEO[ei[i]].vecOptions, vecEO[ei[i]].timeOut);
                job_queue_set_name(&jq, ei[i], engines[i].name.buf);
            }

        Game game = game_init(job.round, job.game);

        // Choose opening position
        int color = 0;
        bool ok = false;

        while (!ok) {
            openings_next(&openings, &fen, options.repeat ? idx / 2 : idx);
            ok = game_load_fen(&game, fen.buf, &color);

            if (!ok) {
                stdio_lock(stdout); // lock both stderr and stdout to prevent interleaving
                fprintf(stderr, "[%d] Illegal FEN '%s'\n", threadId, fen.buf);
                stdio_unlock(stdout);
            }
        }

        const int whiteIdx = color ^ job.reverse;

        printf("[%d] Started game %zu of %zu (%s vs %s)\n", threadId, idx + 1, count,
               engines[whiteIdx].name.buf, engines[opposite(whiteIdx)].name.buf);

        const EngineOptions *eoPair[2] = {&vecEO[ei[0]], &vecEO[ei[1]]};
        const int wld = game_play(w, &game, &options, engines, eoPair, job.reverse);

        // Write to PGN file
        if (options.pgn.len) {
            scope(str_destroy) str_t pgnText = str_init();
            game_export_pgn(&game, options.pgnVerbosity, &pgnText);
            seq_writer_push(&pgnSeqWriter, idx, pgnText);
        }

        // Write to Sample file
        if (options.sp.fileName.len)
            game_export_samples(&game, sampleFile, options.sp.bin);

        // Write to stdout a one line summary of the game
        scope(str_destroy) str_t result = str_init(), reason = str_init();
        game_decode_state(&game, &result, &reason);

        printf("[%d] Finished game %zu (%s vs %s): %s {%s}\n", threadId, idx + 1,
               engines[whiteIdx].name.buf, engines[opposite(whiteIdx)].name.buf, result.buf,
               reason.buf);

        // Pair update
        int wldCount[3] = {0};
        job_queue_add_result(&jq, job.pair, wld, wldCount);
        const int n = wldCount[RESULT_WIN] + wldCount[RESULT_LOSS] + wldCount[RESULT_DRAW];
        printf("Score of %s vs %s: %d - %d - %d  [%.3f] %d\n", engines[0].name.buf,
               engines[1].name.buf, wldCount[RESULT_WIN], wldCount[RESULT_LOSS],
               wldCount[RESULT_DRAW], (wldCount[RESULT_WIN] + 0.5 * wldCount[RESULT_DRAW]) / n, n);

        // SPRT update
        if (options.sprt && sprt_done(wldCount, &options.sprtParam))
            job_queue_stop(&jq);

        // Tournament update
        if (vec_size(vecEO) > 2)
            job_queue_print_results(&jq, (size_t)options.games);

        game_destroy(&game);
    }

    for (int i = 0; i < 2; i++)
        engine_destroy(w, &engines[i]);

    return NULL;
}

int main(int argc, const char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "-version")) {
        puts("c-chess-cli " VERSION);
        return 0;
    }

    main_init(argc, argv);

    // Start threads[]
    pthread_t threads[options.concurrency];

    for (int i = 0; i < options.concurrency; i++)
        pthread_create(&threads[i], NULL, thread_start, &vecWorkers[i]);

    // Main thread loop: check deadline overdue at regular intervals
    do {
        system_sleep(100);

        // We want some tolerance on small delays here. Given a choice, it's best to wait for the
        // worker thread to notice an overdue deadline, which it will handled nicely by counting the
        // game as lost for the offending engine, and continue. Enforcing deadlines from the master
        // thread is the last resort solution, because it is an unrecovrable error. At this point we
        // are likely to face a completely unresponsive engine, where any attempt at I/O will block
        // the master thread, on top of the already blocked worker. Hence, we must DIE().
        for (int i = 0; i < options.concurrency; i++)
            if (deadline_overdue(&vecWorkers[i])) {
                DIE_IF(fprintf(vecWorkers[i].log,
                               "deadline_clear: now is T1=%" PRId64
                               ". %s responded after T0+D=%" PRId64 ". fatal error!\n",
                               system_msec(), vecWorkers[i].deadline.engineName.buf,
                               vecWorkers[i].deadline.timeLimit) < 0);
                DIE("[%d] engine %s is unresponsive\n", vecWorkers[i].id,
                    vecWorkers[i].deadline.engineName.buf);
            }
    } while (!job_queue_done(&jq));

    // Join threads[]
    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
