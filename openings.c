#include "openings.h"
#include "util.h"

Openings openings_new(const char *fileName, bool randomStart)
{
    Openings openings;

    if (*fileName) {
        if (!(openings.file = fopen(fileName, "r")))
            die("openings_create() failed to open '%s'\n", fileName);
    } else
        openings.file = NULL;

    if (openings.file && randomStart) {
        // Start from random position in the file
        fseek(openings.file, 0, SEEK_END);
        uint64_t seed = system_msec();
        const uint64_t size = ftell(openings.file);

        if (!size)
            die("openings_create(): file size = 0");

        fseek(openings.file, prng(&seed) % size, SEEK_SET);

        // Consume current line, likely broken, as we're somewhere in the middle of it
        str_t fen = str_new();
        openings_get(&openings, &fen);
        str_delete(&fen);
    }

    return openings;
}

void openings_delete(Openings *openings)
{
    if (openings->file)
        fclose(openings->file);
}

void openings_get(Openings *openings, str_t *fen)
{
    if (!openings->file) {
        str_cpy(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");  // start pos
        return;
    }

    str_t line = str_new();
    flockfile(openings->file);

    if (!str_getline(&line, openings->file, true)) {
        // Try (once) to wrap around EOF
        rewind(openings->file);

        if (!str_getline(&line, openings->file, true))
            die("openings_get(): cannot read a single line");
    }

    funlockfile(openings->file);
    str_tok(line.buf, fen, ";");
    str_delete(&line);
}
