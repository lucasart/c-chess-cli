/*
 * c-chess-cli, a command line interface for UCI chess engines. Copyright 2020 lucasart.
 *
 * c-chess-cli is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * c-chess-cli is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include "options.h"
#include "util.h"

// Parse a value in="a:b" into out[]=(a, b), or (a,a) if ":" is missing. Note that a and b may
// contain a ':', if escaped with a backslash, eg. in='foo\:bar:baz' => (a='foo:bar', b='baz').
static void split_engine_option(const char *in, str_t out[2])
{
    in = str_tok_esc(in, &out[0], ':', '\\');
    in = str_tok_esc(in, &out[1], ':', '\\');

    if (!in)
        str_cpy(&out[1], out[0]);
}

Options options_new(int argc, const char **argv, GameOptions *go)
{
    // List options that expect a value
    static const char *options[] = {"-concurrency", "-games", "-openings", "-pgnout", "-cmd",
        "-name", "-options", "-nodes", "-depth", "-draw", "-resign", "-movetime", "-tc", "-sprt",
        "-sample"};

    // Set default values
    Options o = {0};
    o.concurrency = 1;
    o.games = 1;
    o.alpha = o.beta = 0.05;

    int i;
    bool expectValue = false;  // pattern: '-tag [value]'. should next arg be a value or tag ?

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && isalpha((unsigned char)argv[i][1])) {
            // process tag
            if (expectValue)
                DIE("value expected after '%s'. found tag '%s' instead.\n", argv[i - 1], argv[i]);

            for (size_t j = 0; j < sizeof(options) / sizeof(*options); j++)
                if (!strcmp(argv[i], options[j])) {
                    expectValue = true;
                    break;
                }

            if (!expectValue) {
                // process tag without value (bool)
                if (!strcmp(argv[i], "-random"))
                    o.random = true;
                else if (!strcmp(argv[i], "-repeat"))
                    o.repeat = true;
                else if (!strcmp(argv[i], "-log"))
                    o.log = true;
                else
                    DIE("invalid tag '%s'\n", argv[i]);
            }
        } else {
            // Process a value
            if (!expectValue)
                DIE("tag expected after '%s'. found value '%s' instead.\n", argv[i - 1], argv[i]);

            if (!strcmp(argv[i - 1], "-concurrency"))
                o.concurrency = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-games"))
                o.games = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-openings"))
                str_cpy_c(&o.openings, argv[i]);
            else if (!strcmp(argv[i - 1], "-pgnout"))
                str_cpy_c(&o.pgnOut, argv[i]);
            else if (!strcmp(argv[i - 1], "-cmd"))
                split_engine_option(argv[i], o.cmd);
            else if (!strcmp(argv[i - 1], "-name"))
                split_engine_option(argv[i], o.name);
            else if (!strcmp(argv[i - 1], "-options"))
                split_engine_option(argv[i], o.uciOptions);
            else if (!strcmp(argv[i - 1], "-nodes")) {
                str_t nodes[2] = {{0}, {0}};
                split_engine_option(argv[i], nodes);
                go->nodes[0] = (uint64_t)atoll(nodes[0].buf);
                go->nodes[1] = (uint64_t)atoll(nodes[1].buf);
                str_del_n(&nodes[0], &nodes[1]);
            } else if (!strcmp(argv[i - 1], "-depth")) {
                str_t depth[2] = {{0}, {0}};
                split_engine_option(argv[i], depth);
                go->depth[0] = atoi(depth[0].buf);
                go->depth[1] = atoi(depth[1].buf);
                str_del_n(&depth[0], &depth[1]);
            } else if (!strcmp(argv[i - 1], "-movetime")) {
                str_t movetime[2] = {{0}, {0}};
                split_engine_option(argv[i], movetime);
                go->movetime[0] = (int64_t)(atof(movetime[0].buf) * 1000);
                go->movetime[1] = (int64_t)(atof(movetime[1].buf) * 1000);
                str_del_n(&movetime[0], &movetime[1]);
            } else if (!strcmp(argv[i - 1], "-resign"))
                sscanf(argv[i], "%i,%i", &go->resignCount, &go->resignScore);
            else if (!strcmp(argv[i - 1], "-draw"))
                sscanf(argv[i], "%i,%i", &go->drawCount, &go->drawScore);
            else if (!strcmp(argv[i - 1], "-tc")) {
                str_t tc[2] = {{0}, {0}};
                split_engine_option(argv[i], tc);

                for (int j = 0; j < 2; j++) {
                    double time = 0, increment = 0;

                    if (strchr(tc[j].buf, '/'))
                        sscanf(tc[j].buf, "%i/%lf+%lf", &go->movestogo[j], &time, &increment);
                    else
                        sscanf(tc[j].buf, "%lf+%lf", &time, &increment);

                    go->time[j] = (int64_t)(time * 1000);
                    go->increment[j] = (int64_t)(increment * 1000);
                }

                str_del_n(&tc[0], &tc[1]);
            } else if (!strcmp(argv[i - 1], "-sprt")) {
                o.sprt = true;
                sscanf(argv[i], "%lf,%lf,%lf,%lf", &o.elo0, &o.elo1, &o.alpha, &o.beta);
            } else if (!strcmp(argv[i - 1], "-sample")) {
                scope(str_del) str_t token = {0};
                const char *tail = str_tok(argv[i], &token, ",");
                assert(tail);

                // Parse sample frequency (and check range)
                go->sampleFrequency = atof(token.buf);
                if (go->sampleFrequency > 1.0 || go->sampleFrequency < 0.0)
                    DIE("Sample frequency '%f' must be between 0 and 1\n", go->sampleFrequency);

                // Parse filename (default sample.csv if omitted)
                if ((tail = str_tok(tail, &token, ",")))
                    o.sampleFileName = str_dup(token);
                else
                    o.sampleFileName = str_dup_c("sample.csv");
            } else
                assert(false);

            expectValue = false;
        }
    }

    if (expectValue)
        DIE("value expected after '%s'\n", argv[i - 1]);

   return o;
}

void options_delete(Options *o)
{
    str_del_n(&o->openings, &o->pgnOut, &o->sampleFileName);

    for (int i = 0; i < 2; i++)
        str_del_n(&o->uciOptions[i], &o->cmd[i], &o->name[i]);
}
