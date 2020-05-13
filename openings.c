#include "openings.h"
#include "util.h"

Openings openings_new(const char *fileName, int rounds, bool random)
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
        str_t fen = str_new();
        openings_get(&o, &fen);
        str_delete(&fen);
    }

    pthread_mutex_init(&o.mtx, NULL);
    o.roundsLeft = rounds;

    return o;
}

void openings_delete(Openings *o)
{
    if (o->file)
        fclose(o->file);

    pthread_mutex_destroy(&o->mtx);
}

bool openings_get(Openings *o, str_t *fen)
{
    if (!o->file) {
        pthread_mutex_lock(&o->mtx);
        const bool done = o->roundsLeft-- <= 0;
        pthread_mutex_unlock(&o->mtx);

        str_cpy(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");  // start pos
        return !done;
    }

    str_t line = str_new();

    pthread_mutex_lock(&o->mtx);

    if (!str_getline(&line, o->file, true)) {
        // Try (once) to wrap around EOF
        rewind(o->file);

        if (!str_getline(&line, o->file, true))
            die("openings_get(): cannot read a single line");
    }

    const bool done = o->roundsLeft-- <= 0;

    pthread_mutex_unlock(&o->mtx);

    str_tok(line.buf, fen, ";");
    str_delete(&line);

    return !done;
}
