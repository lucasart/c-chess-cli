#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"
#include "util.h"

Options options_new(int argc, const char **argv, int start)
{
    // Set default values
    Options options = {
        .chess960 = false,
        .concurrency = 1,
        .games = 1,
        .openings = str_new(),
        .pgnout = str_new(),
        .uciOptions = str_new(),
        .random = false,
        .repeat = false,
        .debug = false
    };

    int i;  // iterator for argv[]
    bool expectValue = false;  // pattern: '-tag [value]'. should next token to be a value or tag ?

    for (i = start; i < argc; i++) {
        if (argv[i][0] == '-') {
            // process tag
            if (expectValue)
                die("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            if (strstr("-concurrency -games -openings -pgnout -ucioptions", argv[i]))
                // process tags followed by value
                expectValue = true;
            else {
                // process tag without value (bool)
                if (!strcmp(argv[i], "-chess960"))
                    options.chess960 = true;
                else if (!strcmp(argv[i], "-random"))
                    options.random = true;
                else if (!strcmp(argv[i], "-repeat"))
                    options.repeat = true;
                else if (!strcmp(argv[i], "-debug"))
                    options.debug = true;
                else
                    die("invalid tag '%s'\n", argv[i]);
            }
        } else {
            // Process a value
            if (!expectValue)
                die("tag expected after '%s'. found value '%s' instead.\n", argv[i - 1], argv[i]);

            if (!strcmp(argv[i - 1], "-concurrency"))
                options.concurrency = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-games"))
                options.games = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-openings"))
                str_cpy(&options.openings, argv[i]);
            else if (!strcmp(argv[i - 1], "-pgnout"))
                str_cpy(&options.pgnout, argv[i]);
            else if (!strcmp(argv[i - 1], "-ucioptions")) {
                str_cpy(&options.uciOptions, argv[i]);

                // Duplicate UCI option list, if only one is given
                if (!strchr(options.uciOptions.buf, ':'))
                    str_cat(&options.uciOptions, ":", options.uciOptions.buf);
            } else
                assert(false);

            expectValue = false;
        }
    }

    if (expectValue)
        die("value expected after '%s'\n", argv[i - 1]);

   return options;
}

void options_delete(Options *options)
{
    str_delete(&options->openings, &options->pgnout, &options->uciOptions);
}
