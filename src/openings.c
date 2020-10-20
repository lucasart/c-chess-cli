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
#include "openings.h"
#include "util.h"
#include "vec.h"

Openings openings_new(str_t fileName, bool random, bool repeat, int threadId)
{
    Openings o = {0};

    if (fileName.len)
        DIE_IF(threadId, !(o.file = fopen(fileName.buf, "r")));
    else
        o.file = NULL;

    if (o.file) {
        // Builds o.index[] to record file offsets for each lines
        o.index = vec_new(1, size_t);
        scope(str_del) str_t line = str_new();

        do {
            vec_push(o.index, ftell(o.file));
        } while (str_getline(&line, o.file));

        vec_pop(o.index);  // EOF offset must be removed

        if (random) {
            // Shuffle o.index[], which will be read sequentially from the beginning. This allows
            // consistent treatment of random and !random, and guarantees no repetition N-cycles in
            // the random case, rather than sqrt(N) (birthday paradox) if random seek each time.
            const size_t n = vec_size(o.index);
            uint64_t seed = (uint64_t)system_msec();

            for (size_t i = n - 1; i > 0; i--) {
                const size_t j = prng(&seed) % (i + 1);
                swap(o.index[i], o.index[j]);
            }
        }
    }

    pthread_mutex_init(&o.mtx, NULL);
    o.repeat = repeat;

    return o;
}

void openings_delete(Openings *o, int threadId)
{
    if (o->file)
        DIE_IF(threadId, fclose(o->file) < 0);

    pthread_mutex_destroy(&o->mtx);
    vec_del(o->index);
}

int openings_next(Openings *o, str_t *fen, int threadId)
{
    if (!o->file) {
        pthread_mutex_lock(&o->mtx);
        const size_t count = ++o->count;
        pthread_mutex_unlock(&o->mtx);

        str_cpy_c(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        return count;
    }

    pthread_mutex_lock(&o->mtx);

    // Read 'fen' from file
    scope(str_del) str_t line = str_new();
    DIE_IF(threadId, fseek(o->file, o->index[o->pos], SEEK_SET) < 0);
    DIE_IF(threadId, !str_getline(&line, o->file));
    str_tok(line.buf, fen, ";");

    const size_t count = ++o->count;

    if (!o->repeat || count % 2 == 0)
        o->pos = (o->pos + 1) % vec_size(o->index);

    pthread_mutex_unlock(&o->mtx);
    return count;
}
