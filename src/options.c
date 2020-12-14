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
#include <string.h>
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
        str_cpy(&o->sample, token);
    else
        str_cpy_c(&o->sample, "sample.csv");
}

// Parse time control. Expects 'mtg/time+inc' or 'time+inc'. Note that time and inc are provided by
// the user in seconds, instead of msec.
static void options_parse_tc(const char *s, EngineOptions *eo)
{
    double time = 0, increment = 0;

    // s = left+increment
    scope(str_destroy) str_t left = str_init(), right = str_init();
    str_tok(str_tok(s, &left, "+"), &right, "+");
    increment = atof(right.buf);

    // parse left
    if (strchr(left.buf, '/')) {
        // left = movestogo/time
        scope(str_destroy) str_t copy = str_init_from(left);
        str_tok(str_tok(copy.buf, &left, "/"), &right, "/");
        eo->movestogo = atoi(left.buf);
        time = atof(right.buf);
    } else
        // left = time
        time = atof(left.buf);

    eo->time = (int64_t)(time * 1000);
    printf("MEIN: time for %s is %ld\n", eo->name.buf, eo->time);
    eo->increment = (int64_t)(increment * 1000);
}

static int options_parse_eo(int argc, const char **argv, int i, EngineOptions *eo)
{
    while (i < argc && argv[i][0] != '-') {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[i], "cmd=")))
            str_cpy_c(&eo->cmd, tail);
        else if ((tail = str_prefix(argv[i], "name=")))
            str_cpy_c(&eo->name, tail);
        else if ((tail = str_prefix(argv[i], "option.")))
            vec_push(eo->options, str_init_from_c(tail));  // store "name=value" string
        else if ((tail = str_prefix(argv[i], "depth=")))
            eo->depth = atoi(tail);
        else if ((tail = str_prefix(argv[i], "nodes=")))
            eo->nodes = atoll(tail);
        else if ((tail = str_prefix(argv[i], "st=")))
            eo->movetime = (int64_t)(atof(tail) * 1000);
        else if ((tail = str_prefix(argv[i], "tc=")))
            options_parse_tc(tail, eo);
        else
            DIE("Illegal syntax '%s'\n", argv[i]);

        i++;
    }

    return i - 1;
}

static int options_parse_openings(int argc, const char **argv, int i, Options *o)
{
    while (i < argc && argv[i][0] != '-') {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[i], "file=")))
            str_cpy_c(&o->openings, tail);
        else if ((tail = str_prefix(argv[i], "order="))) {
            if (!strcmp(tail, "random"))
                o->random = true;
            else if (strcmp(tail, "sequential"))
                DIE("Invalid order for -openings: '%s'\n", tail);
        } else if ((tail = str_prefix(argv[i], "srand=")))
            o->srand = (uint64_t)atoll(tail);
        else
            DIE("Illegal token in -openings: '%s'\n", argv[i]);

        i++;
    }

    return i - 1;
}

static int options_parse_adjudication(int argc, const char **argv, int i, int *count, int *score)
{
    if (i + 1 < argc) {
        *count = atoi(argv[i++]);
        *score = atoi(argv[i]);
    } else
        DIE("Missing parameter(s) for '%s'\n", argv[i - 1]);

    return i;
}

static int options_parse_sprt(int argc, const char **argv, int i, Options *o)
{
    o->sprt = true;

    while (i < argc && argv[i][0] != '-') {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[i], "elo0=")))
            o->sprtParam.elo0 = atof(tail);
        else if ((tail = str_prefix(argv[i], "elo1=")))
            o->sprtParam.elo1 = atof(tail);
        else if ((tail = str_prefix(argv[i], "alpha=")))
            o->sprtParam.alpha = atof(tail);
        else if ((tail = str_prefix(argv[i], "beta=")))
            o->sprtParam.beta = atof(tail);
        else
            DIE("Illegal token in -sprt: '%s'\n", argv[i]);

        i++;
    }

    if (!sprt_validate(&o->sprtParam))
        DIE("Invalid SPRT parameters\n");

    return i - 1;
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
    o.sample = str_init();

    // non-zero default values
    o.concurrency = 1;
    o.games = o.rounds = 1;
    o.sprtParam.alpha = o.sprtParam.beta = 0.05;
    o.pgnVerbosity = 3;

    return o;
}

void options_parse(int argc, const char **argv, Options *o, EngineOptions **eo)
{
    scope(engine_options_destroy) EngineOptions each = engine_options_init();
    bool eachSet = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-repeat"))
            o->repeat = true;
        else if (!strcmp(argv[i], "-gauntlet"))
            o->gauntlet = true;
        else if (!strcmp(argv[i], "-log"))
            o->log = true;
        else if (!strcmp(argv[i], "-concurrency"))
            o->concurrency = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-each")) {
            i = options_parse_eo(argc, argv, i + 1, &each);
            eachSet = true;
        } else if (!strcmp(argv[i], "-engine")) {
            EngineOptions new = engine_options_init();
            i = options_parse_eo(argc, argv, i + 1, &new);
            vec_push(*eo, new);  // new gets moved here
        } else if (!strcmp(argv[i], "-games"))
            o->games = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-rounds"))
            o->rounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-openings"))
            i = options_parse_openings(argc, argv, i + 1, o);
        else if (!strcmp(argv[i], "-pgn")) {
            str_cpy_c(&o->pgn, argv[++i]);

            if (i + 1 < argc && argv[i + 1][0] != '-')
                o->pgnVerbosity = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-resign"))
            i = options_parse_adjudication(argc, argv, i + 1, &o->resignCount, &o->resignScore);
        else if (!strcmp(argv[i], "-draw"))
            i = options_parse_adjudication(argc, argv, i + 1, &o->drawCount, &o->drawScore);
        else if (!strcmp(argv[i], "-sprt"))
            i = options_parse_sprt(argc, argv, i + 1, o);
        else if (!strcmp(argv[i], "-sample"))
            options_parse_sample(argv[++i], o);
        else
            DIE("Unknown option '%s'\n", argv[i]);
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
    str_destroy_n(&o->openings, &o->pgn, &o->sample);
}
