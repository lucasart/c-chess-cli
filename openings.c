#include "openings.h"
#include "util.h"

static void read_infinite(FILE *in, str_t *line)
// Read from an infinite file (wrap around EOF)
{
    if (!str_getline(line, in, true)) {
        // Failed ? wrap around EOF
        rewind(in);

        // Still fail ? die
        if (!str_getline(line, in, true))
            die("read_infinite(): cannot read a single line\n");
    }
}

Openings openings_new(const char *fileName, bool random, int repeat)
{
    Openings o;

    if (*fileName) {
        if (!(o.file = fopen(fileName, "r")))
            die("openings_create() failed to open '%s'\n", fileName);
    } else
        o.file = NULL;

    if (o.file && random) {
        // Start from random position in the file
        fseek(o.file, 0, SEEK_END);
        uint64_t seed = system_msec();
        const uint64_t size = ftell(o.file);

        if (!size)
            die("openings_create(): file size = 0");

        fseek(o.file, prng(&seed) % size, SEEK_SET);

        // Consume current line, likely broken, as we're somewhere in the middle of it
        str_t line = str_new();
        read_infinite(o.file, &line);
        str_delete(&line);
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
        fclose(o->file);

    pthread_mutex_destroy(&o->mtx);
    str_delete(&o->lastFen);
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
        str_cpy(fen, o->lastFen.buf);
    else {
        // Read 'fen' from file, and save in 'o->lastFen'
        str_t line = str_new();
        read_infinite(o->file, &line);
        str_tok(line.buf, fen, ";");
        str_cpy(&o->lastFen, fen->buf);
        str_delete(&line);
    }

    const int next = ++o->next;

    pthread_mutex_unlock(&o->mtx);
    return next;
}
