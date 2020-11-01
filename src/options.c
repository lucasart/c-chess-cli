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

static void options_parse_sample(const char *s, Options *o)
{
    scope(str_destroy) str_t token = str_init();
    const char *tail = str_tok(s, &token, ",");
    assert(tail);

    // Parse sample frequency (and check range)
    o->sampleFrequency = atof(token.buf);

    if (o->sampleFrequency > 1.0 || o->sampleFrequency < 0.0)
        DIE("Sample frequency '%f' must be between 0 and 1\n", o->sampleFrequency);

    // Parse resolve flag
    if ((tail = str_tok(tail, &token, ",")))
        o->sampleResolvePv = !strcmp(token.buf, "y");

    // Parse filename (default sample.csv if omitted)
    if ((tail = str_tok(tail, &token, ",")))
        str_cpy(&o->sampleFileName, token);
    else
        str_cpy_c(&o->sampleFileName, "sample.csv");
}

// Parse time control. Expects 'mtg/time+inc' or 'time+inc'. Note that time and inc are provided by
// the user in seconds, instead of msec.
static void options_parse_tc(const char *s, EngineOptions *eo)
{
    double time = 0, increment = 0;

    if (strchr(s, '/')) {
        if (sscanf(s, "%i/%lf+%lf", &eo->movestogo, &time, &increment) < 2)
            DIE("Cannot parse 'tc:%s'\n", s);
    } else {
        eo->movestogo = 0;

        if (sscanf(s, "%lf+%lf", &time, &increment) < 1)
            DIE("Cannot parse 'tc:%s'\n", s);
    }

    eo->time = (int64_t)(time * 1000);
    eo->increment = (int64_t)(increment * 1000);
}

static void options_parse_eo(int argc, const char **argv, int *i, EngineOptions *eo)
{
    for (int j = *i + 1; j < argc && argv[j][0] != '-'; j++) {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[j], "cmd=")))
            str_cpy_c(&eo->cmd, tail);
        else if ((tail = str_prefix(argv[j], "name=")))
            str_cpy_c(&eo->name, tail);
        else if ((tail = str_prefix(argv[j], "option.")))
            vec_push(eo->options, str_init_from_c(tail));  // store "name=value" string
        else if ((tail = str_prefix(argv[j], "depth=")))
            eo->depth = atoi(tail);
        else if ((tail = str_prefix(argv[j], "nodes=")))
            eo->nodes = atoll(tail);
        else if ((tail = str_prefix(argv[j], "st=")))
            eo->movetime = (int64_t)(atof(tail) * 1000);
        else if ((tail = str_prefix(argv[j], "tc=")))
            options_parse_tc(tail, eo);
        else
            DIE("illegal syntax '%s'\n", argv[j]);

        (*i)++;
    }
}

EngineOptions engine_options_init(void)
{
    EngineOptions eo = {0};
    eo.cmd = str_init();
    eo.name = str_init();
    eo.options = vec_init(str_t);
    return eo;
}

void engine_options_destroy(EngineOptions *eo)
{
    str_destroy_n(&eo->cmd, &eo->name);
    vec_destroy_rec(eo->options, str_destroy);
}

Options options_init(void)
{
    Options o = {0};
    o.openings = str_init();
    o.pgn = str_init();
    o.sampleFileName = str_init();
    return o;
}

void options_parse(int argc, const char **argv, Options *o, EngineOptions **eo)
{
    // List options that expect a value
    static const char *options[] = {"-concurrency", "-games", "-rounds", "-openings", "-pgn",
        "-nodes", "-depth", "-draw", "-resign", "-st", "-tc", "-sprt", "-sample"};

    // Default values
    o->concurrency = 1;
    o->games = o->rounds = 1;
    o->sprtParam.alpha = o->sprtParam.beta = 0.05;

    scope(engine_options_destroy) EngineOptions each = engine_options_init();
    bool eachSet = false;

    bool expectValue = false;  // pattern: '-tag [value]'. should next arg be a value or tag ?

    for (int i = 1; i < argc; i++) {
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
                    EngineOptions new = engine_options_init();
                    options_parse_eo(argc, argv, &i, &new);
                    vec_push(*eo, new);  // new is moved here (engine_options_destroy(&new) not called)
                } else if (!strcmp(argv[i], "-each")) {
                    options_parse_eo(argc, argv, &i, &each);
                    eachSet = true;
                } else if (!strcmp(argv[i], "-random"))
                    o->random = true;
                else if (!strcmp(argv[i], "-repeat"))
                    o->repeat = true;
                else if (!strcmp(argv[i], "-gauntlet"))
                    o->gauntlet = true;
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
            else if (!strcmp(argv[i - 1], "-rounds"))
                o->rounds = atoi(argv[i]);
            else if (!strcmp(argv[i - 1], "-openings"))
                str_cpy_c(&o->openings, argv[i]);
            else if (!strcmp(argv[i - 1], "-pgn"))
                str_cpy_c(&o->pgn, argv[i]);
            else if (!strcmp(argv[i - 1], "-resign"))
                sscanf(argv[i], "%i,%i", &o->resignCount, &o->resignScore);
            else if (!strcmp(argv[i - 1], "-draw"))
                sscanf(argv[i], "%i,%i", &o->drawCount, &o->drawScore);
            else if (!strcmp(argv[i - 1], "-sprt")) {
                o->sprt = true;
                sscanf(argv[i], "%lf,%lf,%lf,%lf", &o->sprtParam.elo0, &o->sprtParam.elo1,
                    &o->sprtParam.alpha, &o->sprtParam.beta);
            } else if (!strcmp(argv[i - 1], "-sample"))
                options_parse_sample(argv[i], o);
            else
                assert(false);

            expectValue = false;
        }

        if (expectValue && i == argc - 1)
            DIE("value expected after '%s'\n", argv[i]);
    }

    if (eachSet) {
        for (size_t i = 0; i < vec_size(*eo); i++) {
            if (each.cmd.len)
                str_cpy(&(*eo)[i].cmd, each.cmd);

            if (each.name.len)
                str_cpy(&(*eo)[i].name, each.name);

            for (size_t j = 0; j < vec_size(each.options); j++)
                vec_push((*eo)[i].options, str_init_from(each.options[j]));

            if (each.time)
                (*eo)[i].time = each.time;

            if (each.increment)
                (*eo)[i].increment = each.increment;

            if (each.movetime)
                (*eo)[i].movetime = each.movetime;

            if (each.nodes)
                (*eo)[i].nodes = each.nodes;

            if (each.depth)
                (*eo)[i].depth = each.depth;

            if (each.movestogo)
                (*eo)[i].movestogo = each.movestogo;
        }
    }

    if (vec_size(*eo) < 2)
        DIE("at least 2 engines are needed\n");

    if (vec_size(*eo) > 2 && o->sprt)
        DIE("only 2 engines for SPRT\n");
}

void options_destroy(Options *o)
{
    str_destroy_n(&o->openings, &o->pgn, &o->sampleFileName);
}
