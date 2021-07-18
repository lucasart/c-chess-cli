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
#include "openings.h"
#include "util.h"
#include "vec.h"
#include <assert.h>

Openings openings_init(const char *fileName, bool random, uint64_t srand) {
    Openings o = {.vecIndex = vec_init(size_t)};

    if (*fileName)
        DIE_IF(!(o.file = fopen(fileName, "r" FOPEN_TEXT)));

    if (o.file) {
        // Fill o.vecIndex[] to record file offsets for each lines
        scope(str_destroy) str_t line = str_init();

        do {
            vec_push(o.vecIndex, ftell(o.file));
        } while (str_getline(&line, o.file));

        vec_pop(o.vecIndex); // EOF offset must be removed

        if (random) {
            // Shuffle o.vecIndex[], which will be read sequentially from the beginning. This allows
            // consistent treatment of random and !random, and guarantees no repetition N-cycles in
            // the random case, rather than sqrt(N) (birthday paradox) if random seek each time.
            const size_t n = vec_size(o.vecIndex);
            uint64_t seed = srand ? srand : (uint64_t)system_msec();

            for (size_t i = n - 1; i > 0; i--) {
                const size_t j = prng(&seed) % (i + 1);
                swap(o.vecIndex[i], o.vecIndex[j]);
            }
        }
    }

    pthread_mutex_init(&o.mtx, NULL);
    return o;
}

void openings_destroy(Openings *o) {
    if (o->file)
        DIE_IF(fclose(o->file) < 0);

    pthread_mutex_destroy(&o->mtx);
    vec_destroy(o->vecIndex);
}

void openings_next(Openings *o, str_t *fen, size_t idx) {
    if (!o->file) {
        str_cpy_c(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        return;
    }

    // Read 'fen' from file
    scope(str_destroy) str_t line = str_init();

    pthread_mutex_lock(&o->mtx);
    DIE_IF(fseek(o->file, o->vecIndex[idx % vec_size(o->vecIndex)], SEEK_SET) < 0);
    DIE_IF(!str_getline(&line, o->file));
    pthread_mutex_unlock(&o->mtx);

    str_tok(line.buf, fen, ";");
}
