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
static pthread_mutex_t mtxWorkers = PTHREAD_MUTEX_INITIALIZER;

_Atomic(int) WorkersBusy = 0;

Deadline deadline_new(void)
{
    Deadline d = {0};
    pthread_mutex_init(&d.mtx, NULL);
    d.engineName = str_new();
    return d;
}

void deadline_del(Deadline *d)
{
    str_del(&d->engineName);
    pthread_mutex_destroy(&d->mtx);
}

void deadline_set(Worker *w, const char *engineName, int64_t timeLimit)
{
    assert(w && engineName && timeLimit > 0);

    pthread_mutex_lock(&w->deadline.mtx);

    w->deadline.set = true;
    str_cpy_c(&w->deadline.engineName, engineName);
    w->deadline.timeLimit = timeLimit;

    if (w->log)
        DIE_IF(w->id, fprintf(w->log, "deadline: %s must respond by %" PRId64 "\n",
            w->deadline.engineName.buf, w->deadline.timeLimit) < 0);

    pthread_mutex_unlock(&w->deadline.mtx);
}

void deadline_clear(Worker *w)
{
    assert(w);

    pthread_mutex_lock(&w->deadline.mtx);

    w->deadline.set = false;

    if (w->log)
        DIE_IF(w->id, fprintf(w->log, "deadline: %s responded before %" PRId64 "\n",
            w->deadline.engineName.buf, w->deadline.timeLimit) < 0);

    pthread_mutex_unlock(&w->deadline.mtx);
}

int64_t deadline_overdue(Worker *w)
{
    assert(w);

    pthread_mutex_lock(&w->deadline.mtx);
    const int64_t timeLimit = w->deadline.timeLimit;
    pthread_mutex_unlock(&w->deadline.mtx);

    const int64_t time = system_msec();

    if (w->deadline.set && time > timeLimit) {
        if (w->log)
            DIE_IF(w->id, fprintf(w->log, "deadline: %s failed to respond by %" PRId64
                ". Caught by main thread %" PRId64 "ms after.\n", w->deadline.engineName.buf,
                timeLimit, time - timeLimit) < 0);

        return time - timeLimit;
    } else
        return 0;
}

Worker worker_new(int i, const char *logName)
{
    assert(logName);

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

    if (w->log) {
        DIE_IF(0, fclose(w->log) < 0);
        w->log = NULL;
    }
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
