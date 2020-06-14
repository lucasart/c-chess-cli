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

static void read_infinite(FILE *in, str_t *line)
// Read from an infinite file (wrap around EOF)
{
    if (!str_getline(line, in)) {
        // Failed once: maybe EOF ?
        rewind(in);

        // Try again after rewind, now failure is fatal
        DIE_IF(!str_getline(line, in));
    }
}

Openings openings_new(const str_t *fileName, bool random, int repeat)
{
    Openings o;

    if (fileName->len)
        DIE_IF(!(o.file = fopen(fileName->buf, "r")));
    else
        o.file = NULL;

    if (o.file && random) {
        // Establish file size
        long size;
        DIE_IF(fseek(o.file, 0, SEEK_END) < 0);
        DIE_IF((size = ftell(o.file)) < 0);

        if (!size)
            DIE("openings_create(): file size = 0");

        uint64_t seed = (uint64_t)system_msec();
        DIE_IF(fseek(o.file, (long)(prng(&seed) % (uint64_t)size), SEEK_SET) < 0);

        // Consume current line, likely broken, as we're somewhere in the middle of it
        scope(str_del) str_t line = {0};
        read_infinite(o.file, &line);
    }

    pthread_mutex_init(&o.mtx, NULL);
    o.lastFen = (str_t){0};
    o.repeat = repeat;
    o.next = 0;

    return o;
}

void openings_delete(Openings *o)
{
    if (o->file)
        DIE_IF(fclose(o->file) < 0);

    pthread_mutex_destroy(&o->mtx);
    str_del(&o->lastFen);
}

int openings_next(Openings *o, str_t *fen)
{
    if (!o->file) {
        pthread_mutex_lock(&o->mtx);
        const int next = ++o->next;
        pthread_mutex_unlock(&o->mtx);

        str_cpy(fen, str_ref("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
        return next;
    }

    pthread_mutex_lock(&o->mtx);

    if (o->repeat && o->next % 2 == 1) {
        // Repeat last opening
        assert(o->lastFen.len);
        str_cpy(fen, o->lastFen);
    } else {
        // Read 'fen' from file, and save in 'o->lastFen'
        scope(str_del) str_t line = {0};
        read_infinite(o->file, &line);
        str_tok(line.buf, fen, ";");
        str_cpy(&o->lastFen, *fen);
    }

    const int next = ++o->next;

    pthread_mutex_unlock(&o->mtx);
    return next;
}
