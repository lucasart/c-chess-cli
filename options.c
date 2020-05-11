#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"
#include "util.h"

static void parse_engine_option(const char *in, str_t out[2])
// Parses in=foo into out=(foo,foo), and in=foo:bar into out=(foo,bar)
{
    char *c = strchr(in, ':');

    if (c) {
        // break foo:bar -> (foo,bar)
        str_ncpy(&out[0], in, c - in);
        str_ncpy(&out[1], c + 1, strlen(in) - (c + 1 - in));
    } else {
        // duplicate foo -> (foo,foo)
        str_cpy(&out[0], in);
        str_cpy(&out[1], in);
    }
}

Options options_new(int argc, const char **argv)
{
    // Set default values
    Options options = {
        .chess960 = false,
        .concurrency = 1,
        .games = 1,
        .openings = str_new(),
        .pgnout = str_new(),
        .cmd = {str_new(), str_new()},
        .uciOptions = {str_new(), str_new()},
        .random = false,
        .repeat = false,
        .debug = false
    };

    int i;  // iterator for argv[]
    bool expectValue = false;  // pattern: '-tag [value]'. should next token to be a value or tag ?

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // process tag
            if (expectValue)
                die("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            if (strstr("-concurrency -games -openings -pgnout -cmd -ucioptions", argv[i]))
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
            else if (!strcmp(argv[i - 1], "-cmd"))
                parse_engine_option(argv[i], options.cmd);
            else if (!strcmp(argv[i - 1], "-ucioptions"))
                parse_engine_option(argv[i], options.uciOptions);
            else
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
    str_delete(&options->openings, &options->pgnout);
    str_delete(&options->uciOptions[0], &options->uciOptions[1]);
    str_delete(&options->cmd[0], &options->cmd[1]);
}
