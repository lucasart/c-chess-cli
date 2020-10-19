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
static Openings openings;

static void *thread_start(void *arg)
{
    W = (Worker *)arg;
    Engine engines[2];

    // Prepare log file
    FILE *log = NULL;

    scope(str_del) str_t logName = {0};
    str_cat_fmt(&logName, "c-chess-cli.%i.log", W->id);

    if (options.log)
        DIE_IF(W->id, !(log = fopen(logName.buf, "w")));

    // Prepare engines[]
    for (int i = 0; i < 2; i++)
        engines[i] = engine_new(eo[i].cmd, eo[i].name, eo[i].options, log,
            &W->deadline);

    int next;
    scope(str_del) str_t fen = {0};

    while ((next = openings_next(&openings, &fen, W->id)) <= options.games) {
        // Play 1 game
        Game game = game_new(&fen);
        const EngineOptions *eoPair[2] = {&eo[0], &eo[1]};
        const int wld = game_play(&game, W->go, engines, eoPair, &W->deadline,
            next % 2 == 0, &W->seed);

        // Write to PGN file
        if (W->pgnOut) {
            scope(str_del) str_t pgn = game_pgn(&game);
            DIE_IF(W->id, fputs(pgn.buf, W->pgnOut) < 0);
        }

        // Write to Sample file
        if (W->sampleFile) {
            scope(str_del) str_t lines = {0}, sampleFen = {0};

            for (size_t i = 0; i < vec_size(game.samples); i++) {
                pos_get(&game.samples[i].pos, &sampleFen);
                str_cat_fmt(&lines, "%S,%i,%i\n", sampleFen, game.samples[i].score,
                    game.samples[i].result);
            }

            DIE_IF(W->id, fputs(lines.buf, W->sampleFile) < 0);
        }

        // Write to stdout a one line summary of the game
        scope(str_del) str_t reason = {0}, result = game_decode_state(&game, &reason);
        printf("[%i] %s vs %s: %s (%s)\n", W->id, game.names[WHITE].buf,
            game.names[BLACK].buf, result.buf, reason.buf);

        // Update on global score (across workers)
        int wldCount[3];
        workers_add_result(W, wld, wldCount);

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

        game_delete(&game);
    }

    for (int i = 0; i < 2; i++)
        engine_del(&engines[i]);

    if (log)
        DIE_IF(W->id, fclose(log) < 0);

    WorkersBusy--;
    return NULL;
}

int main(int argc, const char **argv)
{
    GameOptions go;
    eo = vec_new(2, EngineOptions);
    options_parse(argc, argv, &options, &go, &eo);

    openings = openings_new(options.openings, options.random, options.repeat, 0);

    FILE *pgnOut = NULL;
    if (options.pgnOut.len)
        DIE_IF(0, !(pgnOut = fopen(options.pgnOut.buf, "a")));

    FILE *sampleFile = NULL;
    if (options.sampleFileName.len)
        DIE_IF(0, !(sampleFile = fopen(options.sampleFileName.buf, "ab")));

    pthread_t threads[options.concurrency];
    workers_new(options.concurrency, pgnOut, sampleFile, &go);

    for (int i = 0; i < options.concurrency; i++) {
        WorkersBusy++;
        pthread_create(&threads[i], NULL, thread_start, &Workers[i]);
    }

    do {
        system_sleep(100);

        for (int i = 0; i < options.concurrency; i++) {
            const Engine *deadEngine = deadline_overdue(&Workers[i].deadline);

            if (deadEngine)
                DIE("[%d] engine %s is unresponsive\n", i, deadEngine->name.buf);
        }
    } while (WorkersBusy > 0);

    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    workers_delete();

    if (pgnOut)
        DIE_IF(0, fclose(pgnOut) < 0);

    if (sampleFile)
        DIE_IF(0, fclose(sampleFile) < 0);

    openings_delete(&openings, 0);
    options_delete(&options, eo);
    return 0;
}
