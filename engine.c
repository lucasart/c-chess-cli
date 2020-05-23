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
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "engine.h"
#include "util.h"

static void engine_spawn(Engine *e, const char *cmd)
{
    // Pipe diagram: Parent -> [1]into[0] -> Child -> [1]outof[0] -> Parent
    // 'into' and 'outof' are pipes, each with 2 ends: read=0, write=1
    int outof[2], into[2];

    e->in = e->out = NULL;  // silence bogus compiler warning

    CHECK(pipe(outof) < 0, -1);
    CHECK(pipe(into) < 0, -1);
    CHECK(e->pid = fork(), -1);

    if (e->pid == 0) {
        // in the child process
        close(into[1]);
        close(outof[0]);

        CHECK(dup2(into[0], STDIN_FILENO), -1);
        CHECK(dup2(outof[1], STDOUT_FILENO), -1);

        close(into[0]);
        close(outof[1]);

        CHECK(execlp(cmd, cmd, NULL), -1);
    } else {
        assert(e->pid > 0);

        // in the parent process
        close(into[0]);
        close(outof[1]);

        CHECK(e->in = fdopen(outof[0], "r"), NULL);
        CHECK(e->out = fdopen(into[1], "w"), NULL);
    }
}

Engine engine_new(const char *cmd, const char *name, FILE *log, const char *uciOptions)
{
    assert(cmd && name && uciOptions);  // log can be NULL

    Engine e;
    e.log = log;
    e.name = str_dup(*name ? name : cmd); // default value

    engine_spawn(&e, cmd);  // spawn child process and plug pipes

    engine_writeln(&e, "uci");
    str_t line = str_new(), token = str_new();

    do {
        engine_readln(&e, &line);
        const char *tail = line.buf;

        // Set e.name, by parsing "id name %s", only if no name was provided (*name == '\0')
        if (!*name && (tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "id")
                && (tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "name") && tail)
            str_cpy(&e.name, tail + 1);
    } while (strcmp(line.buf, "uciok"));

    // Parses uciOptions (eg. "Hash=16,Threads=8"), and set engine options accordingly
    while ((uciOptions = str_tok(uciOptions, &token, ","))) {
        const char *c = strchr(token.buf, '=');
        assert(c);

        str_cpy(&line, "setoption name ");
        str_ncat(&line, token.buf, c - token.buf);
        str_cat(&line, " value ", c + 1);

        engine_writeln(&e, line.buf);
    }

    str_delete(&line, &token);
    return e;
}

void engine_delete(Engine *e)
{
    str_delete(&e->name);
    CHECK(fclose(e->in), EOF);
    CHECK(fclose(e->out), EOF);
    CHECK(kill(e->pid, SIGTERM), -1);
}

void engine_readln(const Engine *e, str_t *line)
{
    CHECK(str_getline(line, e->in), 0);

    if (e->log && fprintf(e->log, "%s -> %s\n", e->name.buf, line->buf) <= 0)
        die("engine_writeln() failed writing to log\n");
}

void engine_writeln(const Engine *e, char *buf)
{
    CHECK(fputs(buf, e->out), EOF);
    CHECK(fputc('\n', e->out), EOF);
    CHECK(fflush(e->out), EOF);

    if (e->log && (fprintf(e->log, "%s <- %s\n", e->name.buf, buf) < 0 || fflush(e->log) != 0))
        die("engine_writeln() failed writing to log\n");
}

void engine_sync(const Engine *e)
{
    engine_writeln(e, "isready");

    str_t line = str_new();

    do {
        engine_readln(e, &line);
    } while (strcmp(line.buf, "readyok"));

    str_delete(&line);
}

bool engine_bestmove(const Engine *e, int *score, int64_t *timeLeft, str_t *best)
{
    int result = false;
    *score = 0;
    str_t line = str_new(), token = str_new();

    const int64_t start = system_msec(), deadline = start + *timeLeft;

    while (*timeLeft >= 0 && !result) {
        engine_readln(e, &line);
        *timeLeft = deadline - system_msec();

        const char *tail = str_tok(line.buf, &token, " ");

        if (!tail)
            ;  // empty line, nothing to do
        else if (!strcmp(token.buf, "info")) {
            while ((tail = str_tok(tail, &token, " ")) && strcmp(token.buf, "score"));

            if ((tail = str_tok(tail, &token, " "))) {
                if (!strcmp(token.buf, "cp") && (tail = str_tok(tail, &token, " ")))
                    *score = atoi(token.buf);
                else if (!strcmp(token.buf, "mate") && (tail = str_tok(tail, &token, " ")))
                    *score = atoi(token.buf) < 0 ? INT_MIN : INT_MAX;
                else
                    die("illegal syntax after 'score' in '%s'\n", line.buf);
            }
        } else if (!strcmp(token.buf, "bestmove") && str_tok(tail, &token, " ")) {
            str_cpy_s(best, &token);
            result = true;
        }
    }

    // Time out. We can't leave the engine in limbo for the next ucinewgame. Stop the search asap.
    if (!result) {
        engine_writeln(e, "stop");

        do {
            engine_readln(e, &line);
        } while (strncmp(line.buf, "bestmove ", strlen("bestmove ")));
    }

    str_delete(&line, &token);
    return result;
}
