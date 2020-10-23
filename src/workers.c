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
#include "vec.h"

Worker *Workers;
_Thread_local Worker *W = NULL;
static pthread_mutex_t mtxWorkers = PTHREAD_MUTEX_INITIALIZER;

_Atomic(int) WorkersBusy = 0;

EngineOptions engine_options_new(void)
{
    EngineOptions eo = {0};
    eo.cmd = str_new();
    eo.name = str_new();
    eo.options = vec_new(1, str_t);
    return eo;
}

void engine_options_del(EngineOptions *eo)
{
    str_del_n(&eo->cmd, &eo->name);
    vec_del_rec(eo->options, str_del);
}

Worker worker_new(int i, const char *logName)
{
    assert(!W && logName);

    Worker w = {0};
    w.seed = (uint64_t)i;
    w.id = i + 1;

    if (*logName) {
        w.log = fopen(logName, "w");
        DIE_IF(0, !w.log);
    }

    w.deadline = deadline_new();
    return w;
}

void worker_del(Worker *w)
{
    deadline_del(&w->deadline);

    if (w->log)
        DIE_IF(0, fclose(w->log) < 0);

    *w = (Worker){0};
}

// TODO: document this function...
void workers_add_result(Worker *w, int result, int wldCount[3])
{
    pthread_mutex_lock(&mtxWorkers);

    // Add wld result to specificied worker
    w->wldCount[result]++;

    // Refresh totals (across workers)
    wldCount[0] = wldCount[1] = wldCount[2] = 0;

    for (size_t i = 0; i < vec_size(Workers); i++)
        for (int j = 0; j < 3; j++)
            wldCount[j] += Workers[i].wldCount[j];

    pthread_mutex_unlock(&mtxWorkers);
}
