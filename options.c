#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"
#include "util.h"

#include <stdio.h>

Options options_parse(int argc, const char **argv)
{
    // Set default values
    Options options = {
        .chess960 = false,
        .concurrency = 1,
        .games = 1,
        .openings = NULL,
        .random = false,
        .repeat = false
    };

    int i;  // iterator for argv[]
    bool expectValue = false;  // pattern: '-tag [value]'. should next token to be a value or tag ?

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // process tag
            if (expectValue)
                die("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            if (strstr("-concurrency -games -openings", argv[i]))
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
            else {
                assert(!strcmp(argv[i - 1], "-openings"));
                options.openings = argv[i];
            }

            expectValue = false;
        }
    }

    if (expectValue)
        die("value expected after '%s'\n", argv[i - 1]);

    printf("chess960=%d, concurrency=%d, games=%d, openings='%s', random=%d, repeat=%d\n",
        options.chess960, options.concurrency, options.games, options.openings, options.random,
        options.repeat);
 
   return options;
}

