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
#include "vec.h"

static void engine_spawn(Engine *e, const char *cwd, const char *run, char **argv,
    bool readStdErr)
{
    // Pipe diagram: Parent -> [1]into[0] -> Child -> [1]outof[0] -> Parent
    // 'into' and 'outof' are pipes, each with 2 ends: read=0, write=1
    int outof[2], into[2];

    e->in = e->out = NULL;  // silence bogus compiler warning

    DIE_IF(e->threadId, pipe(outof) < 0);
    DIE_IF(e->threadId, pipe(into) < 0);
    DIE_IF(e->threadId, (e->pid = fork()) < 0);

    if (e->pid == 0) {
        // in the child process
        DIE_IF(e->threadId, close(into[1]) < 0);
        DIE_IF(e->threadId, close(outof[0]) < 0);

        DIE_IF(e->threadId, dup2(into[0], STDIN_FILENO) < 0);
        DIE_IF(e->threadId, dup2(outof[1], STDOUT_FILENO) < 0);

        // When readStdErr, dump stderr into stdout, like doing '2>&1' in bash
        if (readStdErr)
            DIE_IF(e->threadId, dup2(outof[1], STDERR_FILENO) < 0);

        // Close file descriptors that won't be needed anymore
        DIE_IF(e->threadId, close(into[0]) < 0);
        DIE_IF(e->threadId, close(outof[1]) > 0);

        // Set cwd as current directory, and execute run
        DIE_IF(e->threadId, chdir(cwd) < 0);
        DIE_IF(e->threadId, execvp(run, argv) < 0);
    } else {
        assert(e->pid > 0);

        // in the parent process
        DIE_IF(e->threadId, close(into[0]) < 0);
        DIE_IF(e->threadId, close(outof[1]) < 0);

        DIE_IF(e->threadId, !(e->in = fdopen(outof[0], "r")));
        DIE_IF(e->threadId, !(e->out = fdopen(into[1], "w")));
    }
}

Engine engine_new(str_t cmd, str_t name, str_t uciOptions, FILE *log, Deadline *deadline,
    int threadId)
{
    if (!cmd.len)
        DIE("[%d] missing command to start engine.\n", threadId);

    Engine e;
    e.threadId = threadId;
    e.log = log;
    e.name = str_dup(name.len ? name : cmd); // default value

    /* Shell parsing of cmd */

    // Step 1: isolate the first token being the command to run. The space character is ambiguous,
    // because it is allowed in file names. So we must use str_tok_esc().
    scope(str_del) str_t token = {0};
    const char *tail = cmd.buf;
    tail = str_tok_esc(tail, &token, ' ', '\\');

    // Step 2: now we have the command (without its arguments). This command could be:
    // (a) an unqualified path, like "demolito" (which evecvp() will search in PATH)
    // (b) a qualified path (absolute starting with "/", or relative starting with "./" or "../")
    // For (b), we want to separate into executable and directory, so instead of running
    // "../Engines/demolito" from the cwd, we execute run="./demolito" from cwd="../Engines"
    scope(str_del) str_t cwd = str_dup_c("./"), run = str_dup(token);
    const char *lastSlash = strrchr(token.buf, '/');

    if (lastSlash) {
        str_ncpy(&cwd, token, (size_t)(lastSlash - token.buf));
        str_cpy_c(&run, "./");
        str_cat_c(&run, lastSlash + 1);
    }

    // Step 3: Collect the arguments into a vec of str_t, args[]
    str_t *args = vec_new(1, str_t);
    vec_push(args, str_dup(run));  // argv[0] is the executed command

    while ((tail = str_tok_esc(tail, &token, ' ', '\\')))
        vec_push(args, str_dup(token));

    // Step 4: Obviously, evecvp() doesn't deal in vec of str_t, so prepare a char **, whose
    // elements point to the C-string buffers of the elements of args (with an extra NULL to signal
    // the end of the argv array).
    char **argv = calloc(vec_size(args) + 1, sizeof(char *));

    for (size_t i = 0; i < vec_size(args); i++)
        argv[i] = args[i].buf;

    // Spawn child process and plug pipes
    engine_spawn(&e, cwd.buf, run.buf, argv, log != NULL);

    // Free memory for string elements in the vector, then for the vector itself. Note that this
    // will not happen explicitely in the child process, but that's fine, because execvp() will
    // reclaim the memory.
    for (size_t i = 0; i < vec_size(args); i++)
        str_del(&args[i]);

    vec_del(args);

    // Start the uci..uciok dialogue
    deadline_set(deadline, &e, system_msec() + 1000);
    engine_writeln(&e, "uci");
    scope(str_del) str_t line = {0};

    do {
        engine_readln(&e, &line);
        tail = line.buf;

        // Set e.name, by parsing "id name %s", only if no name was provided (*name == '\0')
        if (!name.len && (tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "id")
                && (tail = str_tok(tail, &token, " ")) && !strcmp(token.buf, "name") && tail)
            str_cpy_c(&e.name, tail + strspn(tail, " "));
    } while (strcmp(line.buf, "uciok"));

    // Parses uciOptions (eg. "Hash=16,Threads=8"), and set engine options accordingly. Option names
    // or values may contain ',' if escaped with a backshash.
    tail = uciOptions.buf;

    while ((tail = str_tok_esc(tail, &token, ',', '\\'))) {
        const char *c = strchr(token.buf, '=');
        assert(c);

        str_cpy_c(&line, "setoption name ");
        str_ncat(&line, token, (size_t)(c - token.buf));
        str_cat_fmt(&line, " value %s", c + 1);

        engine_writeln(&e, line.buf);
    }

    deadline_clear(deadline);
    return e;
}

void engine_del(Engine *e)
{
    str_del(&e->name);
    DIE_IF(e->threadId, fclose(e->in) == EOF);
    DIE_IF(e->threadId, fclose(e->out) == EOF);
    DIE_IF(e->threadId, kill(e->pid, SIGTERM) < 0);
}

void engine_readln(const Engine *e, str_t *line)
{
    if (!str_getline(line, e->in))
        DIE("[%d] could not read from %s\n", e->threadId, e->name.buf);

    if (e->log)
        DIE_IF(e->threadId, fprintf(e->log, "%s -> %s\n", e->name.buf, line->buf) < 0);
}

void engine_writeln(const Engine *e, char *buf)
{
    DIE_IF(e->threadId, fputs(buf, e->out) < 0);
    DIE_IF(e->threadId, fputc('\n', e->out) < 0);
    DIE_IF(e->threadId, fflush(e->out) == EOF);

    if (e->log) {
        DIE_IF(e->threadId, fprintf(e->log, "%s <- %s\n", e->name.buf, buf) < 0);
        DIE_IF(e->threadId, fflush(e->log) < 0);
    }
}

void engine_sync(const Engine *e, Deadline *deadline)
{
    deadline_set(deadline, e, system_msec() + 1000);
    engine_writeln(e, "isready");
    scope(str_del) str_t line = {0};

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
    scope(str_del) str_t line = {0}, token = {0};

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
                    DIE("illegal syntax after 'score' in '%s'\n", line.buf);
            }
        } else if (!strcmp(token.buf, "bestmove") && str_tok(tail, &token, " ")) {
            str_cpy(best, token);
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

void deadline_set(Deadline *deadline, const Engine *e, int64_t timeLimit)
{
    assert(deadline);

    pthread_mutex_lock(&deadline->mtx);
    deadline->engine = e;
    deadline->timeLimit = timeLimit;

    if (e && e->log)
        DIE_IF(e->threadId, fprintf(e->log, "deadline set: %s must respond by %" PRId64 "\n",
            e->name.buf, timeLimit) < 0);

    pthread_mutex_unlock(&deadline->mtx);
}

void deadline_clear(Deadline *deadline)
{
    assert(deadline);

    if (deadline->engine && deadline->engine->log)
        DIE_IF(deadline->engine->threadId, fprintf(deadline->engine->log, "deadline cleared: %s"
            " has no more deadline (was %" PRId64 " previously).\n", deadline->engine->name.buf,
            deadline->timeLimit) < 0);

    deadline_set(deadline, NULL, 0);
}

const Engine *deadline_overdue(Deadline *deadline)
{
    assert(deadline);

    pthread_mutex_lock(&deadline->mtx);
    const int64_t timeLimit = deadline->timeLimit;
    const Engine *e = deadline->engine;
    pthread_mutex_unlock(&deadline->mtx);

    const int64_t time = system_msec();

    if (e && time > timeLimit) {
        if (e->log)
            DIE_IF(e->threadId, fprintf(e->log, "deadline failed: %s responded at %" PRId64 ", %"
                PRId64 "ms after the deadline.\n", e->name.buf, time, time - timeLimit) < 0);

        return e;
    } else {
        if (e && e->log)
            DIE_IF(e->threadId, fprintf(e->log, "deadline ok: %s responded at %" PRId64 ", %"
                PRId64 "ms before the deadline.\n", e->name.buf, time, timeLimit - time) < 0);

        return NULL;
    }
}
