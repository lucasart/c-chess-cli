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
        // options (-tag value)
        .concurrency = 1,
        .games = 1,
        .openings = str_new(),
        .pgnout = str_new(),

        // flags (-tag)
        .chess960 = false,
        .random = false,
        .repeat = false,
        .debug = false,

        // engine options (-tag value1[:value2])
        .cmd = {str_new(), str_new()},
        .uciOptions = {str_new(), str_new()},
        .nodes = {0},
        .depth = {0},
        .movetime = {0}
    };

    int i;  // iterator for argv[]
    bool expectValue = false;  // pattern: '-tag [value]'. should next token to be a value or tag ?

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // process tag
            if (expectValue)
                die("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            if (strstr("-concurrency -games -openings -pgnout -cmd -ucioptions -nodes -depth -movetime", argv[i]))
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
            else if (!strcmp(argv[i - 1], "-nodes")) {
                str_t nodes[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], nodes);
                options.nodes[0] = atoll(nodes[0].buf);
                options.nodes[1] = atoll(nodes[1].buf);
                str_delete(&nodes[0], &nodes[1]);
            } else if (!strcmp(argv[i - 1], "-depth")) {
                str_t depth[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], depth);
                options.depth[0] = atoi(depth[0].buf);
                options.depth[1] = atoi(depth[1].buf);
                str_delete(&depth[0], &depth[1]);
            } else if (!strcmp(argv[i - 1], "-movetime")) {
                str_t movetime[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], movetime);
                options.movetime[0] = atoi(movetime[0].buf);
                options.movetime[1] = atoi(movetime[1].buf);
                str_delete(&movetime[0], &movetime[1]);
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
    str_delete(&options->openings, &options->pgnout);
    str_delete(&options->uciOptions[0], &options->uciOptions[1]);
    str_delete(&options->cmd[0], &options->cmd[1]);
}
