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

        // Consume current line, which is likely broken, as we're somewhere in the middle of it
        str_t line = openings_get(&openings);
        str_delete(&line);
    }

    return openings;
}

void openings_delete(Openings *openings)
{
    if (openings->file)
        fclose(openings->file);
}

str_t openings_get(Openings *openings)
{
    if (!openings->file)
        return str_dup("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");  // start pos

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
    str_delete(&line);
    return fen;
}
