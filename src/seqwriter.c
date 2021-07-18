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
#include "seqwriter.h"
#include "vec.h"
#include <string.h>

static SeqStr seq_str_init(size_t idx, str_t str) {
    return (SeqStr){.idx = idx, .str = str_init_from(str)};
}

static void seq_str_destroy(SeqStr *ss) { str_destroy(&ss->str); }

SeqWriter seq_writer_init(const char *fileName, const char *mode) {
    SeqWriter sw = {.out = fopen(fileName, mode), .vecQueued = vec_init(SeqStr)};

    pthread_mutex_init(&sw.mtx, NULL);
    return sw;
}

void seq_writer_destroy(SeqWriter *sw) {
    pthread_mutex_destroy(&sw->mtx);
    vec_destroy_rec(sw->vecQueued, seq_str_destroy);
    fclose(sw->out);
}

void seq_writer_push(SeqWriter *sw, size_t idx, str_t str) {
    pthread_mutex_lock(&sw->mtx);

    // Append to sw->vecQueued[n]
    const size_t n = vec_size(sw->vecQueued);
    vec_push(sw->vecQueued, seq_str_init(idx, str));

    // insert in correct position
    for (size_t i = 0; i < n; i++)
        if (sw->vecQueued[i].idx > idx) {
            SeqStr tmp = sw->vecQueued[n];
            memmove(&sw->vecQueued[i + 1], &sw->vecQueued[i], (n - i) * sizeof(SeqStr));
            sw->vecQueued[i] = tmp;
            break;
        }

    // Calculate i such that buf[0..i-1] is the longest sequential chunk
    size_t i = 0;
    for (; i < vec_size(sw->vecQueued); i++)
        if (sw->vecQueued[i].idx != sw->idxNext + i) {
            assert(sw->vecQueued[i].idx > sw->idxNext + i);
            break;
        }

    if (i) {
        // Write buf[0..i-1] to file, and destroy elements
        for (size_t j = 0; j < i; j++) {
            fputs(sw->vecQueued[j].str.buf, sw->out);
            seq_str_destroy(&sw->vecQueued[j]);
        }
        fflush(sw->out);

        // Delete buf[0..i-1]
        memmove(&sw->vecQueued[0], &sw->vecQueued[i],
                (vec_size(sw->vecQueued) - i) * sizeof(SeqStr));
        vec_ptr(sw->vecQueued)->size -= i;

        // Updated next expected index
        sw->idxNext += i;
    }

    pthread_mutex_unlock(&sw->mtx);
}
