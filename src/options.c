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
#include "options.h"
#include "util.h"
#include "vec.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

static int options_parse_sample(int argc, const char **argv, int i, Options *o) {
    while (i < argc && argv[i][0] != '-') {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[i], "freq=")))
            o->sp.freq = atof(tail);
        else if ((tail = str_prefix(argv[i], "decay=")))
            o->sp.decay = atof(tail);
        else if ((tail = str_prefix(argv[i], "resolve=")))
            o->sp.resolve = (*tail == 'y');
        else if ((tail = str_prefix(argv[i], "file=")))
            str_cpy_c(&o->sp.fileName, tail);
        else if ((tail = str_prefix(argv[i], "format="))) {
            if (!strcmp(tail, "csv"))
                o->sp.bin = false;
            else if (!strcmp(tail, "bin"))
                o->sp.bin = true;
            else
                DIE("Illegal format in -sample: '%s'\n", tail);
        } else
            DIE("Illegal token in -sample: '%s'\n", argv[i]);

        i++;
    }

    if (!o->sp.fileName.len)
        str_cpy_fmt(&o->sp.fileName, "sample.%s", o->sp.bin ? "bin" : "csv");

    return i - 1;
}

// Parse time control. Expects 'mtg/time+inc' or 'time+inc'. Note that time and inc are provided by
// the user in seconds, instead of msec.
static void options_parse_tc(const char *s, EngineOptions *eo) {
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
    eo->increment = (int64_t)(increment * 1000);
}

static int options_parse_eo(int argc, const char **argv, int i, EngineOptions *eo) {
    while (i < argc && argv[i][0] != '-') {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[i], "cmd=")))
            str_cpy_c(&eo->cmd, tail);
        else if ((tail = str_prefix(argv[i], "name=")))
            str_cpy_c(&eo->name, tail);
        else if ((tail = str_prefix(argv[i], "option.")))
            vec_push(eo->vecOptions, str_init_from_c(tail)); // store "name=value" string
        else if ((tail = str_prefix(argv[i], "depth=")))
            eo->depth = atoi(tail);
        else if ((tail = str_prefix(argv[i], "nodes=")))
            eo->nodes = atoll(tail);
        else if ((tail = str_prefix(argv[i], "movetime=")))
            eo->movetime = (int64_t)(atof(tail) * 1000);
        else if ((tail = str_prefix(argv[i], "tc=")))
            options_parse_tc(tail, eo);
        else if ((tail = str_prefix(argv[i], "timeout=")))
            eo->timeOut = (int64_t)(atof(tail) * 1000);
        else
            DIE("Illegal syntax '%s'\n", argv[i]);

        i++;
    }

    return i - 1;
}

static int options_parse_openings(int argc, const char **argv, int i, Options *o) {
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

static int options_parse_adjudication(int argc, const char **argv, int i, int *number, int *count,
                                      int *score) {
    const int i0 = i;

    while (i < argc && argv[i][0] != '-') {
        const char *tail = NULL;

        if ((tail = str_prefix(argv[i], "number=")))
            *number = atoi(tail);
        else if ((tail = str_prefix(argv[i], "count="))) {
            *count = atoi(tail);
        } else if ((tail = str_prefix(argv[i], "score=")))
            *score = atoi(tail);
        else
            DIE("Illegal token in %s: '%s'\n", argv[i0 - 1], argv[i]);

        i++;
    }

    return i - 1;
}

static int options_parse_sprt(int argc, const char **argv, int i, Options *o) {
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

EngineOptions engine_options_init(void) {
    return (EngineOptions){
        .cmd = str_init(), .name = str_init(), .vecOptions = vec_init(str_t), .timeOut = 4000};
}

void engine_options_destroy(EngineOptions *eo) {
    str_destroy_n(&eo->cmd, &eo->name);
    vec_destroy_rec(eo->vecOptions, str_destroy);
}

static void engine_options_apply(const EngineOptions *from, EngineOptions *to) {
    if (from->cmd.len)
        str_cpy(&to->cmd, from->cmd);

    if (from->name.len)
        str_cpy(&to->name, from->name);

    for (size_t j = 0; j < vec_size(from->vecOptions); j++)
        vec_push(to->vecOptions, str_init_from(from->vecOptions[j]));

    if (from->time)
        to->time = from->time;

    if (from->increment)
        to->increment = from->increment;

    if (from->movetime)
        to->movetime = from->movetime;

    if (from->nodes)
        to->nodes = from->nodes;

    if (from->depth)
        to->depth = from->depth;

    if (from->movestogo)
        to->movestogo = from->movestogo;

    if (from->timeOut)
        to->timeOut = from->timeOut;
}

SampleParams sample_params_init(void) { return (SampleParams){.fileName = str_init(), .freq = 1}; }

void sample_params_destroy(SampleParams *sp) { str_destroy(&sp->fileName); }

Options options_init(void) {
    return (Options){.sp = sample_params_init(),
                     .openings = str_init(),
                     .pgn = str_init(),
                     .concurrency = 1,
                     .games = 1,
                     .rounds = 1,
                     .sprtParam = (SPRTParam){.alpha = 0.05, .beta = 0.05, .elo1 = 4},
                     .pgnVerbosity = 3};
}

void options_destroy(Options *o) {
    sample_params_destroy(&o->sp);
    str_destroy_n(&o->openings, &o->pgn);
}

EngineOptions *options_parse(int argc, const char **argv, Options *o) {
    EngineOptions *vecEO = vec_init(EngineOptions);
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
            vec_push(vecEO, new); // new gets moved here
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
            i = options_parse_adjudication(argc, argv, i + 1, &o->resignNumber, &o->resignCount,
                                           &o->resignScore);
        else if (!strcmp(argv[i], "-draw"))
            i = options_parse_adjudication(argc, argv, i + 1, &o->drawNumber, &o->drawCount,
                                           &o->drawScore);
        else if (!strcmp(argv[i], "-sprt"))
            i = options_parse_sprt(argc, argv, i + 1, o);
        else if (!strcmp(argv[i], "-sample"))
            i = options_parse_sample(argc, argv, i + 1, o);
        else
            DIE("Unknown option '%s'\n", argv[i]);
    }

    if (eachSet) {
        for (size_t i = 0; i < vec_size(vecEO); i++)
            engine_options_apply(&each, &vecEO[i]);
    }

    if (vec_size(vecEO) < 2)
        DIE("at least 2 engines are needed\n");

    if (vec_size(vecEO) > 2 && o->sprt)
        DIE("only 2 engines for SPRT\n");

    return vecEO;
}
