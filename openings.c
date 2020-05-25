#include "openings.h"
#include "util.h"

static void read_infinite(FILE *in, str_t *line)
// Read from an infinite file (wrap around EOF)
{
    if (!str_getline(line, in)) {
        // Failed once: maybe EOF ?
        rewind(in);

        // Try again after rewind, now failure is fatal
        DIE_IF(str_getline(line, in), 0);
    }
}

Openings openings_new(const char *fileName, bool random, int repeat)
{
    Openings o;

    if (*fileName)
        DIE_IF(o.file = fopen(fileName, "r"), NULL);
    else
        o.file = NULL;

    if (o.file && random) {
        // Establish file size
        long size;
        DIE_IF(fseek(o.file, 0, SEEK_END), -1);
        DIE_IF(size = ftell(o.file), -1);

        if (!size)
            die("openings_create(): file size = 0");

        uint64_t seed = system_msec();
        fseek(o.file, prng(&seed) % size, SEEK_SET);

        // Consume current line, likely broken, as we're somewhere in the middle of it
        scope(str_del) str_t line = str_new();
        read_infinite(o.file, &line);
    }

    pthread_mutex_init(&o.mtx, NULL);
    o.lastFen = str_new();
    o.repeat = repeat;
    o.next = 0;

    return o;
}

void openings_delete(Openings *o)
{
    if (o->file)
        DIE_IF(fclose(o->file), EOF);

    pthread_mutex_destroy(&o->mtx);
    str_del(&o->lastFen);
}

int openings_next(Openings *o, str_t *fen)
{
    if (!o->file) {
        pthread_mutex_lock(&o->mtx);
        const int next = ++o->next;
        pthread_mutex_unlock(&o->mtx);

        str_cpy(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");  // start pos
        return next;
    }

    pthread_mutex_lock(&o->mtx);

    if (o->repeat && o->next % 2 == 1)
        // Repeat last opening
        str_cpy_s(fen, &o->lastFen);
    else {
        // Read 'fen' from file, and save in 'o->lastFen'
        scope(str_del) str_t line = str_new();
        read_infinite(o->file, &line);
        str_tok(line.buf, fen, ";");
        str_cpy_s(&o->lastFen, fen);
    }

    const int next = ++o->next;

    pthread_mutex_unlock(&o->mtx);
    return next;
}
