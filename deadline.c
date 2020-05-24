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
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include "deadline.h"
#include "util.h"

void deadline_set(Deadline *deadline, const Engine *engine, int64_t timeLimit)
{
    assert(deadline);

    pthread_mutex_lock(&deadline->mtx);
    deadline->engine = engine;
    deadline->timeLimit = timeLimit;

    if (engine && engine->log)
        fprintf(engine->log, "deadline: %s must respond by %" PRId64 "\n", engine->name.buf,
            timeLimit);

    pthread_mutex_unlock(&deadline->mtx);
}

void deadline_clear(Deadline *deadline)
{
    deadline_set(deadline, NULL, 0);
}

const Engine *deadline_overdue(Deadline *deadline)
{
    assert(deadline);

    pthread_mutex_lock(&deadline->mtx);
    const int64_t timeLimit = deadline->timeLimit;
    const Engine *engine = deadline->engine;
    pthread_mutex_unlock(&deadline->mtx);

    const int64_t time = system_msec();

    if (engine && time > timeLimit) {
        if (engine->log)
            fprintf(engine->log, "deadline: %s failed to respond by %" PRId64 ". now is %" PRId64
                ". overdue by %" PRId64 "ms\n", engine->name.buf, timeLimit, time, time - timeLimit);

        return engine;
    } else
        return NULL;
}
