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

JobQueue job_queue_init(int engines, int rounds, int games)
{
    assert(engines >= 2 && rounds >= 1 && games >= 1);

    JobQueue jq = {0};
    pthread_mutex_init(&jq.mtx, NULL);
    jq.jobs = vec_init(Job);
    jq.results = vec_init(Result);

    // Gauntlet
    for (int r = 0; r < rounds; r++)
        for (int e = 1; e < engines; e++) {
            vec_push(jq.results, (Result){0});

            for (int g = 0; g < games; g++) {
                const Job j = {.e1 = 0, .e2 = e, .reverse = g % 2};
                vec_push(jq.jobs, j);
            }
        }

    return jq;
}

void job_queue_destroy(JobQueue *jq)
{
    vec_destroy(jq->results);
    vec_destroy(jq->jobs);
    pthread_mutex_destroy(&jq->mtx);
}

bool job_queue_pop(JobQueue *jq, Job *j, size_t *idx, size_t *count)
{
    pthread_mutex_lock(&jq->mtx);

    const bool ok = jq->idx < vec_size(jq->jobs);

    if (ok) {
        *j = jq->jobs[jq->idx];
        *idx = jq->idx++;
        *count = vec_size(jq->jobs);
    }

    pthread_mutex_unlock(&jq->mtx);

    return ok;
}

// Add game outcome, and return updated totals
void job_queue_add_result(const JobQueue *jq, int pair, int outcome, int count[3])
{
    pthread_mutex_lock(&jq->results[pair].mtx);

    jq->results[pair].count[outcome]++;
    // memcpy(count, jq->results[pair].count, sizeof(count));
    for (size_t i = 0; i < 3; i++)
        count[i] = jq->results[pair].count[i];

    pthread_mutex_unlock(&jq->results[pair].mtx);
}
