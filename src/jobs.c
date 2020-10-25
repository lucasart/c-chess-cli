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
#include "jobs.h"
#include "vec.h"

JobQueue job_queue_new(int engines, int rounds, int games)
{
    assert(engines >= 2 && rounds >= 1 && games >= 1);

    JobQueue jq = {0};
    pthread_mutex_init(&jq.mtx, NULL);
    jq.jobs = vec_new(Job);

    // Gauntlet: vec_push() in reverse, so we can vec_pop() in order
    for (int r = rounds - 1; r >= 0; r--)
        for (int e = engines - 1; e >= 1; e--)
            for (int g = games - 1; g >= 0; g--) {
                const Job j = {.e1 = 0, .e2 = e, .reverse = g % 2};
                vec_push(jq.jobs, j);
            }

    return jq;
}

void job_queue_del(JobQueue *jq)
{
    vec_del(jq->jobs);
    pthread_mutex_destroy(&jq->mtx);
}

bool job_queue_pop(JobQueue *jq, Job *j)
{
    pthread_mutex_lock(&jq->mtx);

    const bool ok = vec_size(jq->jobs);

    if (ok)
        *j = vec_pop(jq->jobs);

    pthread_mutex_unlock(&jq->mtx);

    return ok;
}
