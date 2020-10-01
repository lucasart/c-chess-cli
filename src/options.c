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
#include "vec.h"

// Parse a value in="a:b" into out[]=(a, b), or (a,a) if ":" is missing. Note that a and b may
// contain a ':', if escaped with a backslash, eg. in='foo\:bar:baz' => (a='foo:bar', b='baz').
static void split_engine_option(const char *in, str_t out[2])
{
    in = str_tok_esc(in, &out[0], ':', '\\');
    in = str_tok_esc(in, &out[1], ':', '\\');

    if (!in)
        str_cpy(&out[1], out[0]);
}

static void options_parse_sample(const char *s, Options *o, GameOptions *go)
{
    scope(str_del) str_t token = {0};
    const char *tail = str_tok(s, &token, ",");
    assert(tail);

    // Parse sample frequency (and check range)
    go->sampleFrequency = atof(token.buf);
    if (go->sampleFrequency > 1.0 || go->sampleFrequency < 0.0)
        DIE("Sample frequency '%f' must be between 0 and 1\n", go->sampleFrequency);

    // Parse resolve flag
    if ((tail = str_tok(tail, &token, ",")))
        go->sampleResolvePv = !strcmp(token.buf, "y");

    // Parse filename (default sample.csv if omitted)
    if ((tail = str_tok(tail, &token, ",")))
        o->sampleFileName = str_dup(token);
    else
        o->sampleFileName = str_dup_c("sample.csv");
}

static void options_parse_tc(const char *s, GameOptions *go)
{
    str_t tc[2] = {{0}, {0}};
    split_engine_option(s, tc);

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
}

void options_parse(int argc, const char **argv, Options *o, GameOptions *go, EngineOptions **eo)
{
    // List options that expect a value
    static const char *options[] = {"-concurrency", "-games", "-openings", "-pgnout", "-nodes",
        "-depth", "-draw", "-resign", "-movetime", "-tc", "-sprt", "-sample"};

    // Set default values
    *go = (GameOptions){0};
    *o = (Options){0};
    o->concurrency = 1;
    o->games = 1;
    o->alpha = o->beta = 0.05;

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
                if (!strcmp(argv[i], "-engine")) {
                    EngineOptions new = {0};

                    for (int j = i + 1; j < argc && argv[j][0] != '-'; j++) {
                        scope(str_del) str_t key = {0}, value = {0};
                        str_tok_esc(str_tok(argv[j], &key, ":"), &value, ':', '\\');
                        assert(key.len);

                        if (!value.len)
                            DIE("expected value after '%s:'\n", key.buf);

                        if (!strcmp(key.buf, "cmd"))
                            new.cmd = str_dup(value);
                        else if (!strcmp(key.buf, "name"))
                            new.name = str_dup(value);
                        else if (!strcmp(key.buf, "options"))
                            new.uciOptions = str_dup(value);
                        else if (!strcmp(key.buf, "depth"))
                            new.depth = atoi(value.buf);
                        else if (!strcmp(key.buf, "nodes"))
                            new.nodes = atoll(value.buf);
                        else if (!strcmp(key.buf, "movetime"))
                            new.movetime = (int64_t)(atof(value.buf) * 1000);
                        else
                            DIE("illegal key '%s'\n", key.buf);

                        i++;
                    }

                    vec_push(*eo, new);
                } else if (!strcmp(argv[i], "-random"))
                    o->random = true;
                else if (!strcmp(argv[i], "-repeat"))
                    o->repeat = true;
                else if (!strcmp(argv[i], "-log"))
                    o->log = true;
                else
                    DIE("invalid tag '%s'\n", argv[i]);
            }
        } else {
            // Process a value
            if (!expectValue)
                DIE("tag expected after '%s'. found value '%s' instead.\n", argv[i - 1], argv[i]);

            if (!strcmp(argv[i - 1], "-concurrency"))
                o->concurrency = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-games"))
                o->games = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-openings"))
                str_cpy_c(&o->openings, argv[i]);
            else if (!strcmp(argv[i - 1], "-pgnout"))
                str_cpy_c(&o->pgnOut, argv[i]);
            else if (!strcmp(argv[i - 1], "-resign"))
                sscanf(argv[i], "%i,%i", &go->resignCount, &go->resignScore);
            else if (!strcmp(argv[i - 1], "-draw"))
                sscanf(argv[i], "%i,%i", &go->drawCount, &go->drawScore);
            else if (!strcmp(argv[i - 1], "-tc"))
                options_parse_tc(argv[i], go);
            else if (!strcmp(argv[i - 1], "-sprt")) {
                o->sprt = true;
                sscanf(argv[i], "%lf,%lf,%lf,%lf", &o->elo0, &o->elo1, &o->alpha, &o->beta);
            } else if (!strcmp(argv[i - 1], "-sample"))
                options_parse_sample(argv[i], o, go);
            else
                assert(false);

            expectValue = false;
        }
    }

    if (expectValue)
        DIE("value expected after '%s'\n", argv[i - 1]);
}

void options_delete(Options *o, EngineOptions *eo)
{
    str_del_n(&o->openings, &o->pgnOut, &o->sampleFileName);

    for (size_t i = 0; i < vec_size(eo); i++)
        str_del_n(&eo[i].cmd, &eo[i].name, &eo[i].uciOptions);

    vec_del(eo);
}
