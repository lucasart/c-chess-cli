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

static void job_queue_init_pair(int games, int e1, int e2, int pair, int *added, int round,
    Job **jobs)
{
    for (int g = 0; g < games; g++) {
        const Job j = {
            .e1 = e1, .e2 = e2, .pair = pair,
            .round = round, .game = (*added)++,
            .reverse = g % 2
        };
        vec_push(*jobs, j);
    }
}

JobQueue job_queue_init(int engines, int rounds, int games, bool gauntlet)
{
    assert(engines >= 2 && rounds >= 1 && games >= 1);

    JobQueue jq = {0};
    pthread_mutex_init(&jq.mtx, NULL);
    jq.jobs = vec_init(Job);
    jq.results = vec_init(Result);

    if (gauntlet) {
        // Gauntlet: N-1 pairs (0, e2) with 0 < e2
        for (int e2 = 1; e2 < engines; e2++)
            vec_push(jq.results, (Result){0});

        for (int r = 0; r < rounds; r++) {
            int added = 0;  // number of games already added to the current round

            for (int e2 = 1; e2 < engines; e2++)
                job_queue_init_pair(games, 0, e2, e2 - 1, &added, r, &jq.jobs);
        }
    } else {
        // Round robin: N(N-1)/2 pairs (e1, e2) with e1 < e2
        for (int e1 = 0; e1 < engines - 1; e1++)
            for (int e2 = e1 + 1; e2 < engines; e2++)
                vec_push(jq.results, (Result){0});

        for (int r = 0; r < rounds; r++) {
            int pair = 0;  // enumerate pairs in order
            int added = 0;  // number of games already added to the current round

            for (int e1 = 0; e1 < engines - 1; e1++)
                for (int e2 = e1 + 1; e2 < engines; e2++)
                    job_queue_init_pair(games, e1, e2, pair++, &added, r, &jq.jobs);
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

    for (size_t i = 0; i < 3; i++)
        count[i] = jq->results[pair].count[i];

    pthread_mutex_unlock(&jq->results[pair].mtx);
}

bool job_queue_done(JobQueue *jq)
{
    pthread_mutex_lock(&jq->mtx);
    assert(jq->idx <= vec_size(jq->jobs));
    const bool done = jq->idx == vec_size(jq->jobs);
    pthread_mutex_unlock(&jq->mtx);

    return done;
}

void job_queue_stop(JobQueue *jq)
{
    pthread_mutex_lock(&jq->mtx);
    jq->idx = vec_size(jq->jobs);
    pthread_mutex_unlock(&jq->mtx);
}
