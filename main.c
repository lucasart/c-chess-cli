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
#include "options.h"

Options options;
EngineOptions engineOptions[2];
pthread_t *threads;

void *thread_start(void *arg)
{
    pthread_t *thread = arg;
    Engine engines[2];

    str_t logName = str_new();
    str_catf(&logName, "c-chess-cli.%d.log", thread - threads);
    FILE *log = options.debug ? fopen(logName.buf, "w") : NULL;
    str_delete(&logName);

    for (int i = 0; i < 2; i++)
        engines[i] = engine_create(engineOptions[i].cmd.buf, log);

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

    threads = calloc(options.concurrency, sizeof(pthread_t));

    for (int i = 0; i < options.concurrency; i++)
        pthread_create(&threads[i], NULL, thread_start, &threads[i]);

    for (int i = 0; i < options.concurrency; i++)
        pthread_join(threads[i], NULL);

    free(threads);

    options_delete(&options);
    str_delete(&engineOptions[0].cmd, &engineOptions[1].cmd);
    return 0;
}
