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
#include "workers.h"
#include <stdio.h>

static void job_queue_init_pair(int games, int e1, int e2, int pair, int *added, int round,
                                Job **jobs) {
    for (int g = 0; g < games; g++) {
        const Job j = {
            .ei = {e1, e2}, .pair = pair, .round = round, .game = (*added)++, .reverse = g % 2};
        vec_push(*jobs, j);
    }
}

JobQueue job_queue_init(int engines, int rounds, int games, bool gauntlet) {
    assert(engines >= 2 && rounds >= 1 && games >= 1);

    JobQueue jq = {
        .vecJobs = vec_init(Job), .vecResults = vec_init(Result), .vecNames = vec_init(str_t)};
    pthread_mutex_init(&jq.mtx, NULL);

    // Prepare engine names: blank for now, will be discovered at run time (concurrently)
    for (int i = 0; i < engines; i++)
        vec_push(jq.vecNames, str_init());

    if (gauntlet) {
        // Gauntlet: N-1 pairs (0, e2) with 0 < e2
        for (int e2 = 1; e2 < engines; e2++) {
            const Result r = {.ei = {0, e2}};
            vec_push(jq.vecResults, r);
        }

        for (int r = 0; r < rounds; r++) {
            int added = 0; // number of games already added to the current round

            for (int e2 = 1; e2 < engines; e2++)
                job_queue_init_pair(games, 0, e2, e2 - 1, &added, r, &jq.vecJobs);
        }
    } else {
        // Round robin: N(N-1)/2 pairs (e1, e2) with e1 < e2
        for (int e1 = 0; e1 < engines - 1; e1++)
            for (int e2 = e1 + 1; e2 < engines; e2++) {
                const Result r = {.ei = {e1, e2}};
                vec_push(jq.vecResults, r);
            }

        for (int r = 0; r < rounds; r++) {
            int pair = 0;  // enumerate pairs in order
            int added = 0; // number of games already added to the current round

            for (int e1 = 0; e1 < engines - 1; e1++)
                for (int e2 = e1 + 1; e2 < engines; e2++)
                    job_queue_init_pair(games, e1, e2, pair++, &added, r, &jq.vecJobs);
        }
    }

    return jq;
}

void job_queue_destroy(JobQueue *jq) {
    vec_destroy(jq->vecResults);
    vec_destroy(jq->vecJobs);
    vec_destroy_rec(jq->vecNames, str_destroy);
    pthread_mutex_destroy(&jq->mtx);
}

bool job_queue_pop(JobQueue *jq, Job *j, size_t *idx, size_t *count) {
    pthread_mutex_lock(&jq->mtx);
    const bool ok = jq->idx < vec_size(jq->vecJobs);

    if (ok) {
        *j = jq->vecJobs[jq->idx];
        *idx = jq->idx++;
        *count = vec_size(jq->vecJobs);
    }

    pthread_mutex_unlock(&jq->mtx);
    return ok;
}

// Add game outcome, and return updated totals
void job_queue_add_result(JobQueue *jq, int pair, int outcome, int count[3]) {
    pthread_mutex_lock(&jq->mtx);
    jq->vecResults[pair].count[outcome]++;
    jq->completed++;

    for (size_t i = 0; i < 3; i++)
        count[i] = jq->vecResults[pair].count[i];

    pthread_mutex_unlock(&jq->mtx);
}

bool job_queue_done(JobQueue *jq) {
    pthread_mutex_lock(&jq->mtx);
    assert(jq->idx <= vec_size(jq->vecJobs));
    const bool done = jq->idx == vec_size(jq->vecJobs);
    pthread_mutex_unlock(&jq->mtx);
    return done;
}

void job_queue_stop(JobQueue *jq) {
    pthread_mutex_lock(&jq->mtx);
    jq->idx = vec_size(jq->vecJobs);
    pthread_mutex_unlock(&jq->mtx);
}

void job_queue_set_name(JobQueue *jq, int ei, const char *name) {
    pthread_mutex_lock(&jq->mtx);

    if (!jq->vecNames[ei].len)
        str_cpy_c(&jq->vecNames[ei], name);

    pthread_mutex_unlock(&jq->mtx);
}

void job_queue_print_results(JobQueue *jq, size_t frequency) {
    pthread_mutex_lock(&jq->mtx);

    if (jq->completed && jq->completed % frequency == 0) {
        scope(str_destroy) str_t out = str_init_from_c("Tournament update:\n");

        for (size_t i = 0; i < vec_size(jq->vecResults); i++) {
            const Result r = jq->vecResults[i];
            const int n = r.count[RESULT_WIN] + r.count[RESULT_LOSS] + r.count[RESULT_DRAW];

            if (n) {
                char score[8] = "";
                sprintf(score, "%.3f", (r.count[RESULT_WIN] + 0.5 * r.count[RESULT_DRAW]) / n);
                str_cat_fmt(&out, "%S vs %S: %i - %i - %i  [%s] %i\n", jq->vecNames[r.ei[0]],
                            jq->vecNames[r.ei[1]], r.count[RESULT_WIN], r.count[RESULT_LOSS],
                            r.count[RESULT_DRAW], score, n);
            }
        }

        fputs(out.buf, stdout);
    }

    pthread_mutex_unlock(&jq->mtx);
}
