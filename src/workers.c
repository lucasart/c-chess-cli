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
#include "workers.h"
#include "util.h"
#include "vec.h"
#include <stdlib.h>

Worker *vecWorkers;

void deadline_set(Worker *w, const char *engineName, int64_t now, int64_t duration) {
    assert((uint64_t)now + (uint64_t)duration > (uint64_t)now); // signed overflow is undefined

    pthread_mutex_lock(&w->deadline.mtx);

    w->deadline.set = true;
    str_cpy_c(&w->deadline.engineName, engineName);
    w->deadline.timeLimit = now + duration;

    pthread_mutex_unlock(&w->deadline.mtx);

    if (w->log)
        DIE_IF(fprintf(w->log,
                       "deadline_set: now is T0=%" PRId64
                       ". %s must respond in less than D=%" PRId64 ".\n",
                       now, engineName, duration) < 0);
}

void deadline_clear(Worker *w) {
    pthread_mutex_lock(&w->deadline.mtx);

    w->deadline.set = false;

    if (w->log)
        DIE_IF(fprintf(w->log,
                       "deadline_clear: now is T1=%" PRId64 ". %s responded before T0+D=%" PRId64
                       ".\n",
                       system_msec(), w->deadline.engineName.buf, w->deadline.timeLimit) < 0);

    pthread_mutex_unlock(&w->deadline.mtx);
}

bool deadline_overdue(Worker *w) {
    pthread_mutex_lock(&w->deadline.mtx);

    const int64_t timeLimit = w->deadline.timeLimit;
    const bool set = w->deadline.set;

    pthread_mutex_unlock(&w->deadline.mtx);

    return set && system_msec() > timeLimit;
}

Worker worker_init(int i, const char *logName) {
    Worker w = {.seed = (uint64_t)i, .id = i + 1};

    pthread_mutex_init(&w.deadline.mtx, NULL);
    w.deadline.engineName = str_init();

    if (*logName) {
        w.log = fopen(logName, "w" FOPEN_TEXT);
        DIE_IF(!w.log);
    }

    return w;
}

void worker_destroy(Worker *w) {
    str_destroy(&w->deadline.engineName);
    pthread_mutex_destroy(&w->deadline.mtx);

    if (w->log) {
        DIE_IF(fclose(w->log) < 0);
        w->log = NULL;
    }
}
