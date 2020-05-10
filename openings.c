#include "openings.h"
#include "util.h"

Openings openings_new(const char *fileName, bool randomStart)
{
    Openings openings;
    openings.file = fopen(fileName, "r");

    if (!openings.file)
        die("openings_create() failed");

    if (randomStart) {
        // Start from random position in the file
        fseek(openings.file, 0, SEEK_END);
        uint64_t seed = system_msec();
        const uint64_t size = ftell(openings.file);

        if (!size)
            die("openings_create(): file size = 0");

        fseek(openings.file, prng(&seed) % size, SEEK_SET);

        // Consume current line, which is likely broken, as we're somewhere in the middle of it
        str_t line = openings_get(&openings);
        str_delete(&line);
    }

    return openings;
}

void openings_delete(Openings *openings)
{
    fclose(openings->file);
}

str_t openings_get(Openings *openings)
{
    str_t line = str_new(), fen = str_new();

    flockfile(openings->file);

    if (!str_getline(&line, openings->file)) {
        // Try (once) to wrap around EOF
        rewind(openings->file);

        if (!str_getline(&line, openings->file))
            die("openings_get(): cannot read a single line");
    }

    funlockfile(openings->file);

    str_tok(line.buf, &fen, ";\n");
    // TODO: proper FEN validation

    str_delete(&line);
    return fen;
}
