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

void deadline_set(Worker *w, const char *engineName, int64_t timeLimit)
{
    assert(timeLimit > 0);

    pthread_mutex_lock(&w->deadline.mtx);

    w->deadline.set = true;
    str_cpy_c(&w->deadline.engineName, engineName);
    w->deadline.timeLimit = timeLimit;

    pthread_mutex_unlock(&w->deadline.mtx);

    if (w->log)
        DIE_IF(w->id, fprintf(w->log, "deadline: %s must respond by %" PRId64 "\n", engineName,
            timeLimit) < 0);
}

void deadline_clear(Worker *w)
{
    pthread_mutex_lock(&w->deadline.mtx);

    w->deadline.set = false;

    if (w->log)
        DIE_IF(w->id, fprintf(w->log, "deadline: %s responded before %" PRId64 "\n",
            w->deadline.engineName.buf, w->deadline.timeLimit) < 0);

    pthread_mutex_unlock(&w->deadline.mtx);
}

int64_t deadline_overdue(Worker *w)
{
    pthread_mutex_lock(&w->deadline.mtx);

    const int64_t timeLimit = w->deadline.timeLimit;
    const bool set = w->deadline.set;

    pthread_mutex_unlock(&w->deadline.mtx);

    const int64_t time = system_msec();

    if (set && time > timeLimit)
        return time - timeLimit;
    else
        return 0;
}

Worker worker_init(int i, const char *logName)
{
    Worker w = {
        .seed = (uint64_t)i,
        .id = i + 1
    };

    pthread_mutex_init(&w.deadline.mtx, NULL);
    w.deadline.engineName = str_init();

    if (*logName) {
        w.log = fopen(logName, "w" FOPEN_TEXT);
        DIE_IF(0, !w.log);
    }

    return w;
}

void worker_destroy(Worker *w)
{
    str_destroy(&w->deadline.engineName);
    pthread_mutex_destroy(&w->deadline.mtx);

    if (w->log) {
        DIE_IF(0, fclose(w->log) < 0);
        w->log = NULL;
    }
}
