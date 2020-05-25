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
#include <pthread.h>
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

    DIE_IF(pipe(outof) < 0, -1);
    DIE_IF(pipe(into) < 0, -1);
    DIE_IF(e->pid = fork(), -1);

    if (e->pid == 0) {
        // in the child process
        close(into[1]);
        close(outof[0]);

        DIE_IF(dup2(into[0], STDIN_FILENO), -1);
        DIE_IF(dup2(outof[1], STDOUT_FILENO), -1);

        close(into[0]);
        close(outof[1]);

        DIE_IF(execlp(cmd, cmd, NULL), -1);
    } else {
        assert(e->pid > 0);

        // in the parent process
        close(into[0]);
        close(outof[1]);

        DIE_IF(e->in = fdopen(outof[0], "r"), NULL);
        DIE_IF(e->out = fdopen(into[1], "w"), NULL);
    }
}

Engine engine_new(const char *cmd, const char *name, FILE *log, Deadline *deadline,
    const char *uciOptions)
{
    assert(cmd && name && uciOptions);  // log can be NULL

    Engine e;
    e.log = log;
    e.name = str_dup(*name ? name : cmd); // default value
    engine_spawn(&e, cmd);  // spawn child process and plug pipes

    deadline_set(deadline, &e, system_msec() + 1000);
    engine_writeln(&e, "uci");

    RAII str_t line = str_new(), token = str_new();

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

    deadline_clear(deadline);
    return e;
}

void engine_delete(Engine *e)
{
    str_del(&e->name);
    DIE_IF(fclose(e->in), EOF);
    DIE_IF(fclose(e->out), EOF);
    DIE_IF(kill(e->pid, SIGTERM), -1);
}

void engine_readln(const Engine *e, str_t *line)
{
    DIE_IF(str_getline(line, e->in), 0);

    if (e->log && fprintf(e->log, "%s -> %s\n", e->name.buf, line->buf) <= 0)
        die("engine_writeln() failed writing to log\n");
}

void engine_writeln(const Engine *e, char *buf)
{
    DIE_IF(fputs(buf, e->out), EOF);
    DIE_IF(fputc('\n', e->out), EOF);
    DIE_IF(fflush(e->out), EOF);

    if (e->log && (fprintf(e->log, "%s <- %s\n", e->name.buf, buf) < 0 || fflush(e->log) != 0))
        die("engine_writeln() failed writing to log\n");
}

void engine_sync(const Engine *e, Deadline *deadline)
{
    deadline_set(deadline, e, system_msec() + 1000);
    engine_writeln(e, "isready");
    RAII str_t line = str_new();

    do {
        engine_readln(e, &line);
    } while (strcmp(line.buf, "readyok"));

    deadline_clear(deadline);
}

bool engine_bestmove(const Engine *e, int *score, int64_t *timeLeft, Deadline *deadline,
    str_t *best)
{
    int result = false;
    *score = 0;
    RAII str_t line = str_new(), token = str_new();

    const int64_t start = system_msec(), timeLimit = start + *timeLeft;
    deadline_set(deadline, e, timeLimit + 1000);

    while (*timeLeft >= 0 && !result) {
        engine_readln(e, &line);
        *timeLeft = timeLimit - system_msec();

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

    // Time out. Send "stop" and give the opportunity to the engine to respond with bestmove (still
    // under deadline protection).
    if (!result) {
        engine_writeln(e, "stop");

        do {
            engine_readln(e, &line);
        } while (strncmp(line.buf, "bestmove ", strlen("bestmove ")));
    }

    deadline_clear(deadline);
    return result;
}

void deadline_set(Deadline *deadline, const Engine *engine, int64_t timeLimit)
{
    assert(deadline);

    pthread_mutex_lock(&deadline->mtx);
    deadline->engine = engine;
    deadline->timeLimit = timeLimit;

    if (engine && engine->log)
        fprintf(engine->log, "deadline set: %s must respond by %" PRId64 "\n", engine->name.buf,
            timeLimit);

    pthread_mutex_unlock(&deadline->mtx);
}

void deadline_clear(Deadline *deadline)
{
    assert(deadline);

    if (deadline->engine && deadline->engine->log)
        fprintf(deadline->engine->log, "deadline cleared: %s has no more deadline (was %" PRId64
            " previously).\n", deadline->engine->name.buf, deadline->timeLimit);

    deadline_set(deadline, NULL, 0);
}

const Engine *deadline_overdue(Deadline *deadline)
{
    assert(deadline);

    pthread_mutex_lock(&deadline->mtx);
    const int64_t timeLimit = deadline->timeLimit;
    const Engine *engine = deadline->engine;
    pthread_mutex_unlock(&deadline->mtx);

    const int64_t time = system_msec();

    if (engine && time > timeLimit) {
        if (engine->log)
            fprintf(engine->log, "deadline failed: %s responded at %" PRId64 ", which is %" PRId64
                "ms after the deadline.\n", engine->name.buf, time, time - timeLimit);

        return engine;
    } else {
        if (engine && engine->log)
            fprintf(engine->log, "deadline passed: %s responded at %" PRId64 ", which is %" PRId64
                "ms before the deadline.\n", engine->name.buf, time, timeLimit - time);

        return NULL;
    }
}
