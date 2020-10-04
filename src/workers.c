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
#include <stdlib.h>
#include "workers.h"
#include "util.h"

Worker *Workers;
_Thread_local Worker *W = NULL;
static int WorkersCount = 0;
static pthread_mutex_t mtxWorkers = PTHREAD_MUTEX_INITIALIZER;

_Atomic(int) WorkersBusy = 0;

void engine_options_del(EngineOptions *eo)
{
    str_del_n(&eo->cmd, &eo->name, &eo->uciOptions);
}

void workers_new(int count, FILE *pgnOut, FILE *sampleFile, const GameOptions *go)
{
    WorkersCount = count;
    Workers = calloc((size_t)count, sizeof(Worker));

    for (int i = 0; i < count; i++) {
        pthread_mutex_init(&Workers[i].deadline.mtx, NULL);
        Workers[i].seed = (uint64_t)i;
        Workers[i].id = i + 1;
        Workers[i].pgnOut = pgnOut;
        Workers[i].sampleFile = sampleFile;
        Workers[i].go = go;
    }
}

void workers_delete()
{
    for (int i = 0; i < WorkersCount; i++)
        pthread_mutex_destroy(&Workers[i].deadline.mtx);

    free(Workers);
    Workers = NULL;
    WorkersCount = 0;
}

// TODO: document this function...
void workers_add_result(Worker *worker, int result, int wldCount[3])
{
    pthread_mutex_lock(&mtxWorkers);

    // Add wld result to specificied worker
    worker->wldCount[result]++;

    // Refresh totals (across workers)
    wldCount[0] = wldCount[1] = wldCount[2] = 0;

    for (int i = 0; i < WorkersCount; i++)
        for (int j = 0; j < 3; j++)
            wldCount[j] += Workers[i].wldCount[j];

    pthread_mutex_unlock(&mtxWorkers);
}
