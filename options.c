#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"
#include "util.h"

static void parse_engine_option(const char *in, str_t out[2], char delim, bool duplicate)
// Parses in=foo into out=(foo,foo), and in=foo:bar into out=(foo,bar)
{
    char *c = strchr(in, delim);

    if (c) {
        // break foo:bar -> (foo,bar)
        str_ncpy(&out[0], in, c - in);
        str_ncpy(&out[1], c + 1, strlen(in) - (c + 1 - in));
    } else if (duplicate) {
        // duplicate foo -> (foo,foo)
        str_cpy(&out[0], in);
        str_cpy(&out[1], in);
    } else
        die("could not parse '%s'\n", in);
}

Options options_new(int argc, const char **argv)
{
    // Set default values
    Options o = {
        // options (-tag value)
        .concurrency = 1,
        .games = 1,
        .openings = str_new(),
        .pgnout = str_new(),

        // flags (-tag)
        .random = false,
        .repeat = false,
        .debug = false,

        // engine options (-tag value1[:value2])
        .cmd = {str_new(), str_new()},
        .uciOptions = {str_new(), str_new()},

        // Game options
        .go = {
            .resignCount = INT_MAX,
            .resignScore = 0,
            .drawCount = INT_MAX,
            .drawScore = 0,
            .nodes = {0},
            .depth = {0},
            .movetime = {0},
            .chess960 = false
        }
    };

    int i;  // iterator for argv[]
    bool expectValue = false;  // pattern: '-tag [value]'. should next token to be a value or tag ?

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // process tag
            if (expectValue)
                die("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            if (strstr("-concurrency -games -openings -pgnout -cmd -ucioptions -nodes -depth -draw -resign"
                    "-movetime", argv[i]))
                // process tags followed by value
                expectValue = true;
            else {
                // process tag without value (bool)
                if (!strcmp(argv[i], "-chess960"))
                    o.go.chess960 = true;
                else if (!strcmp(argv[i], "-random"))
                    o.random = true;
                else if (!strcmp(argv[i], "-repeat"))
                    o.repeat = true;
                else if (!strcmp(argv[i], "-debug"))
                    o.debug = true;
                else
                    die("invalid tag '%s'\n", argv[i]);
            }
        } else {
            // Process a value
            if (!expectValue)
                die("tag expected after '%s'. found value '%s' instead.\n", argv[i - 1], argv[i]);

            if (!strcmp(argv[i - 1], "-concurrency"))
                o.concurrency = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-games"))
                o.games = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-openings"))
                str_cpy(&o.openings, argv[i]);
            else if (!strcmp(argv[i - 1], "-pgnout"))
                str_cpy(&o.pgnout, argv[i]);
            else if (!strcmp(argv[i - 1], "-cmd"))
                parse_engine_option(argv[i], o.cmd, ':', true);
            else if (!strcmp(argv[i - 1], "-ucioptions"))
                parse_engine_option(argv[i], o.uciOptions, ':', true);
            else if (!strcmp(argv[i - 1], "-nodes")) {
                str_t nodes[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], nodes, ':', true);
                o.go.nodes[0] = atoi(nodes[0].buf);
                o.go.nodes[1] = atoi(nodes[1].buf);
                str_delete(&nodes[0], &nodes[1]);
            } else if (!strcmp(argv[i - 1], "-depth")) {
                str_t depth[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], depth, ':', true);
                o.go.depth[0] = atoi(depth[0].buf);
                o.go.depth[1] = atoi(depth[1].buf);
                str_delete(&depth[0], &depth[1]);
            } else if (!strcmp(argv[i - 1], "-movetime")) {
                str_t movetime[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], movetime, ':', true);
                o.go.movetime[0] = atoi(movetime[0].buf);
                o.go.movetime[1] = atoi(movetime[1].buf);
                str_delete(&movetime[0], &movetime[1]);
            } else if (!strcmp(argv[i - 1], "-resign")) {
                str_t resign[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], resign, ',', false);
                o.go.resignCount = atoi(resign[0].buf);
                o.go.resignScore = atoi(resign[1].buf);
                str_delete(&resign[0], &resign[1]);
            } else if (!strcmp(argv[i - 1], "-draw")) {
                str_t draw[2] = {str_new(), str_new()};
                parse_engine_option(argv[i], draw, ',', false);
                o.go.drawCount = atoi(draw[0].buf);
                o.go.drawScore = atoi(draw[1].buf);
                str_delete(&draw[0], &draw[1]);
            } else
                assert(false);

            expectValue = false;
        }
    }

    if (expectValue)
        die("value expected after '%s'\n", argv[i - 1]);

   return o;
}

void options_delete(Options *o)
{
    str_delete(&o->openings, &o->pgnout);
    str_delete(&o->uciOptions[0], &o->uciOptions[1]);
    str_delete(&o->cmd[0], &o->cmd[1]);
}
