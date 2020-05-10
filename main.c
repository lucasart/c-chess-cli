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

Options options;
EngineOptions engineOptions[2];
Openings openings;
FILE *pgnout;
pthread_t *threads;

void *thread_start(void *arg)
{
    pthread_t *thread = arg;
    const int threadId = thread - threads;  // starts at 0
    Engine engines[2];

    str_t logName = str_new();
    str_catf(&logName, "c-chess-cli.%d.log", threadId);
    FILE *log = options.debug ? fopen(logName.buf, "w") : NULL;

    const char *tail = options.uciOptions.buf;
    str_t uciOptions = str_new();

    for (int i = 0; i < 2; i++) {
        tail = str_tok(tail, &uciOptions, ":");
        assert(tail);
        engines[i] = engine_create(engineOptions[i].cmd.buf, log, uciOptions.buf);
    }

    str_delete(&logName, &uciOptions);

    for (int i = 0; i < options.games; i++) {
        str_t fen = openings_get(&openings);
        Game game = game_new(options.chess960, fen.buf);

        game_play(&game, &engines[i % 2], &engines[(i + 1) % 2]);

        // Write to stdout a one line summary
        str_t result = str_new(), reason = str_new();
        result = game_decode_result(&game, &reason);
        printf("[%d] %s vs. %s: %s (%s)\n", threadId, game.names[WHITE].buf, game.names[BLACK].buf,
            result.buf, reason.buf);
        str_delete(&result, &reason);

        // Write to PGN file
        if (pgnout) {
            str_t pgn = game_pgn(&game);
            fputs(pgn.buf, pgnout);
            str_delete(&pgn);
        }

        str_delete(&fen);
        game_delete(&game);
    }

    for (int i = 0; i < 2; i++)
        engine_delete(&engines[i]);

    if (log)
        fclose(log);

    return NULL;
}

int main(int argc, const char **argv)
{
    engineOptions[0].cmd = str_dup(argv[1]);
    engineOptions[1].cmd = str_dup(argv[2]);

    options = options_new(argc, argv, 3);
    openings = openings_new(options.openings.buf, options.random);
    pgnout = options.pgnout.len ? fopen(options.pgnout.buf, "w") : NULL;

    threads = calloc(options.concurrency, sizeof(pthread_t));

    for (int i = 0; i < options.concurrency; i++)
        pthread_create(&threads[i], NULL, thread_start, &threads[i]);

    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    free(threads);

    if (pgnout)
        fclose(pgnout);

    openings_delete(&openings);
    options_delete(&options);
    str_delete(&engineOptions[0].cmd, &engineOptions[1].cmd);
    return 0;
}
