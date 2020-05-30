#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"
#include "util.h"

static void split_engine_option(const char *in, str_t out[2])
{
    char *c = strchr(in, ':');

    if (c) {
        // break foo:bar -> (foo,bar)
        str_ncpy(&out[0], in, (size_t)(c - in));
        str_ncpy(&out[1], c + 1, strlen(in) - (size_t)(c + 1 - in));
    } else {
        // duplicate foo -> (foo,foo)
        str_cpy(&out[0], in);
        str_cpy(&out[1], in);
    }
}

Options options_new(int argc, const char **argv)
{
    // Set default values
    Options o;
    memset(&o, 0, sizeof(o));

    o.concurrency = 1;
    o.games = 1;
    o.openings = str_new();
    o.pgnout = str_new();
    o.alpha = o.beta = 0.05;

    for (int i = 0; i < 2; i++) {
        o.cmd[i] = str_new();
        o.name[i] = str_new();
        o.uciOptions[i] = str_new();
    }

    int i;
    bool expectValue = false;  // pattern: '-tag [value]'. should next arg be a value or tag ?

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // process tag
            if (expectValue)
                die("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            if (strstr("-concurrency -games -openings -pgnout -cmd -name -options -nodes -depth "
                    "-draw -resign -movetime -tc -sprt", argv[i]))
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
                split_engine_option(argv[i], o.cmd);
            else if (!strcmp(argv[i - 1], "-name"))
                split_engine_option(argv[i], o.name);
            else if (!strcmp(argv[i - 1], "-options"))
                split_engine_option(argv[i], o.uciOptions);
            else if (!strcmp(argv[i - 1], "-nodes")) {
                str_t nodes[2] = {str_new(), str_new()};
                split_engine_option(argv[i], nodes);
                o.go.nodes[0] = (uint64_t)atoll(nodes[0].buf);
                o.go.nodes[1] = (uint64_t)atoll(nodes[1].buf);
                str_delete(&nodes[0], &nodes[1]);
            } else if (!strcmp(argv[i - 1], "-depth")) {
                str_t depth[2] = {str_new(), str_new()};
                split_engine_option(argv[i], depth);
                o.go.depth[0] = atoi(depth[0].buf);
                o.go.depth[1] = atoi(depth[1].buf);
                str_delete(&depth[0], &depth[1]);
            } else if (!strcmp(argv[i - 1], "-movetime")) {
                str_t movetime[2] = {str_new(), str_new()};
                split_engine_option(argv[i], movetime);
                o.go.movetime[0] = atof(movetime[0].buf) * 1000;
                o.go.movetime[1] = atof(movetime[1].buf) * 1000;
                str_delete(&movetime[0], &movetime[1]);
            } else if (!strcmp(argv[i - 1], "-resign"))
                sscanf(argv[i], "%i,%i", &o.go.resignCount, &o.go.resignScore);
            else if (!strcmp(argv[i - 1], "-draw"))
                sscanf(argv[i], "%i,%i", &o.go.drawCount, &o.go.drawScore);
            else if (!strcmp(argv[i - 1], "-tc")) {
                str_t tc[2] = {str_new(), str_new()};
                split_engine_option(argv[i], tc);

                for (int j = 0; j < 2; j++) {
                    double time = 0, increment = 0;

                    if (strchr(tc[j].buf, '/'))
                        sscanf(tc[j].buf, "%i/%lf+%lf", &o.go.movestogo[j], &time, &increment);
                    else
                        sscanf(tc[j].buf, "%lf+%lf", &time, &increment);

                    o.go.time[j] = time * 1000;
                    o.go.increment[j] = increment * 1000;
                }

                str_delete(&tc[0], &tc[1]);
            } else if (!strcmp(argv[i - 1], "-sprt")) {
                o.sprt = true;
                sscanf(argv[i], "%lf,%lf,%lf,%lf", &o.elo0, &o.elo1, &o.alpha, &o.beta);
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

    for (int i = 0; i < 2; i++)
        str_delete(&o->uciOptions[i], &o->cmd[i], &o->name[i]);
}
