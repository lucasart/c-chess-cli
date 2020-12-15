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
#ifdef __linux__
    #define _GNU_SOURCE
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/prctl.h>
#else
    #include <unistd.h>
#endif

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include "engine.h"
#include "util.h"
#include "vec.h"

static void engine_spawn(const Worker *w, Engine *e, const char *cwd, const char *run, char **argv,
    bool readStdErr)
{
    assert(argv[0]);

    // Pipe diagram: Parent -> [1]into[0] -> Child -> [1]outof[0] -> Parent
    // 'into' and 'outof' are pipes, each with 2 ends: read=0, write=1
    int outof[2] = {0}, into[2] = {0};

#ifdef __linux__
    DIE_IF(w->id, pipe2(outof, O_CLOEXEC) < 0);
    DIE_IF(w->id, pipe2(into, O_CLOEXEC) < 0);
#else
    DIE_IF(w->id, pipe(outof) < 0);
    DIE_IF(w->id, pipe(into) < 0);
#endif

    DIE_IF(w->id, (e->pid = fork()) < 0);

    if (e->pid == 0) {
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGHUP);  // delegate zombie purge to the kernel
#endif
        // Plug stdin and stdout
        DIE_IF(w->id, dup2(into[0], STDIN_FILENO) < 0);
        DIE_IF(w->id, dup2(outof[1], STDOUT_FILENO) < 0);

        // For stderr we have 2 choices:
        // - readStdErr=true: dump it into stdout, like doing '2>&1' in bash. This is useful, if we
        // want to see error messages from engines in their respective log file (notably assert()
        // writes to stderr). Of course, such error messages should not be UCI commands, otherwise we
        // will be fooled into parsing them as such.
        // - readStdErr=false: do nothing, which means stderr is inherited from the parent process.
        // Typcically, this means all engines write their error messages to the terminal (unless
        // redirected otherwise).
        if (readStdErr)
            DIE_IF(w->id, dup2(outof[1], STDERR_FILENO) < 0);

#ifndef __linux__
        // Ugly (and slow) workaround for non-Linux POSIX systems that lack the ability to
        // atomically set O_CLOEXEC when creating pipes.
        for (int fd = 3; fd < sysconf(FOPEN_MAX); close(fd++));
#endif

        // Set cwd as current directory, and execute run with argv[]
        DIE_IF(w->id, chdir(cwd) < 0);
        DIE_IF(w->id, execvp(run, argv) < 0);
    } else {
        assert(e->pid > 0);

        // in the parent process
        DIE_IF(w->id, close(into[0]) < 0);
        DIE_IF(w->id, close(outof[1]) < 0);

        DIE_IF(w->id, !(e->in = fdopen(outof[0], "r")));
        DIE_IF(w->id, !(e->out = fdopen(into[1], "w")));
    }
}

static void engine_parse_cmd(const char *cmd, str_t *cwd, str_t *run, str_t **args)
{
    // Isolate the first token being the command to run.
    scope(str_destroy) str_t token = str_init();
    const char *tail = cmd;
    tail = str_tok_esc(tail, &token, ' ', '\\');

    // Split token into (cwd, run). Possible cases:
    // (a) unqualified path, like "demolito" (which evecvp() will search in PATH)
    // (b) qualified path (absolute starting with "/", or relative starting with "./" or "../")
    // For (b), we want to separate into executable and directory, so instead of running
    // "../Engines/demolito" from the cwd, we execute run="./demolito" from cwd="../Engines"
    str_cpy_c(cwd, "./");
    str_cpy(run, token);
    const char *lastSlash = strrchr(token.buf, '/');

    if (lastSlash) {
        str_ncpy(cwd, token, (size_t)(lastSlash - token.buf));
        str_cpy_fmt(run, "./%s", lastSlash + 1);
    }

    // Collect the arguments into a vec of str_t, args[]
    vec_push(*args, str_init_from(*run));  // argv[0] is the executed command

    while ((tail = str_tok_esc(tail, &token, ' ', '\\')))
        vec_push(*args, str_init_from(token));
}

Engine engine_init(Worker *w, const char *cmd, const char *name, const str_t *options, int proto)
{
    if (!*cmd)
        DIE("[%d] missing command to start engine.\n", w->id);

    Engine e = {0};
    e.name = str_init_from_c(*name ? name : cmd); // default value
    e.proto = proto;

    // Parse cmd into (cwd, run, args): we want to execute run from cwd with args.
    scope(str_destroy) str_t cwd = str_init(), run = str_init();
    str_t *args = vec_init(str_t);
    engine_parse_cmd(cmd, &cwd, &run, &args);

    // execvp() needs NULL terminated char **, not vec of str_t. Prepare a char **, whose elements
    // point to the C-string buffers of the elements of args, with the required NULL at the end.
    char **argv = calloc(vec_size(args) + 1, sizeof(char *));

    for (size_t i = 0; i < vec_size(args); i++)
        argv[i] = args[i].buf;

    // Spawn child process and plug pipes
    engine_spawn(w, &e, cwd.buf, run.buf, argv, w->log != NULL);

    vec_destroy_rec(args, str_destroy);
    free(argv);

    scope(str_destroy) str_t line = str_init();

    if (proto == PROTO_UCI) {
        // Start the uci..uciok dialogue
        deadline_set(w, e.name.buf, system_msec() + 4000);
        engine_writeln(w, &e, "uci");

        do {
            engine_readln(w, &e, &line);
            const char *tail = NULL;

            // If no name was provided, parse it from "id name %s"
            if (!*name && (tail = str_prefix(line.buf, "id name ")))
                str_cpy_c(&e.name, tail + strspn(tail, " "));

            if ((tail = str_prefix(line.buf, "option name UCI_Chess960 ")))
                e.supportChess960 = true;
        } while (strcmp(line.buf, "uciok"));

        deadline_clear(w);

        
        for (size_t i = 0; i < vec_size(options); i++) {
            scope(str_destroy) str_t oname = str_init(), ovalue = str_init();
            str_tok(str_tok(options[i].buf, &oname, "="), &ovalue, "=");
            str_cpy_fmt(&line, "setoption name %S value %S", oname, ovalue);
            engine_writeln(w, &e, line.buf);
        }
    } else {
        // Wait xboard engine features enumeration
        deadline_set(w, e.name.buf, system_msec() + 4000);
        engine_writeln(w, &e, "xboard");
        engine_writeln(w, &e, "protover 2");
        do {
            engine_readln(w, &e, &line);
            // Todo: add here options such as ping, sigint...
        } while (strcmp(line.buf, "feature done=1"));
        deadline_clear(w);
    }
    return e;
}

void engine_destroy(Worker *w, Engine *e)
{
    // Order the engine to quit, and grant 1s deadline for obeying
    deadline_set(w, e->name.buf, system_msec() + 1000);
    engine_writeln(w, e, "quit");
    waitpid(e->pid, NULL, 0);
    deadline_clear(w);

    str_destroy(&e->name);
    DIE_IF(w->id, fclose(e->in) < 0);
    DIE_IF(w->id, fclose(e->out) < 0);
}

void engine_readln(const Worker *w, const Engine *e, str_t *line)
{
    if (!str_getline(line, e->in))
        DIE("[%d] could not read from %s\n", w->id, e->name.buf);

    if (w->log)
        DIE_IF(w->id, fprintf(w->log, "%s -> %s\n", e->name.buf, line->buf) < 0);
}

void engine_writeln(const Worker *w, const Engine *e, char *buf)
{
    DIE_IF(w->id, fputs(buf, e->out) < 0);
    DIE_IF(w->id, fputc('\n', e->out) < 0);
    DIE_IF(w->id, fflush(e->out) < 0);

    if (w->log) {
        DIE_IF(w->id, fprintf(w->log, "%s <- %s\n", e->name.buf, buf) < 0);
        DIE_IF(w->id, fflush(w->log) < 0);
    }
}

void engine_sync(Worker *w, const Engine *e)
{
    if (e->proto != PROTO_UCI)
        return;  // todo: ping/pong for xboard
    deadline_set(w, e->name.buf, system_msec() + 1000);
    engine_writeln(w, e, "isready");
    scope(str_destroy) str_t line = str_init();

    do {
        engine_readln(w, e, &line);
    } while (strcmp(line.buf, "readyok"));

    deadline_clear(w);
}

static bool uci_engine_bestmove(Worker *w, const Engine *e, int64_t *timeLeft, str_t *best, str_t *pv,
    Info *info)
{
    int result = false;
    scope(str_destroy) str_t line = str_init(), token = str_init();
    str_clear(pv);

    const int64_t start = system_msec(), timeLimit = start + *timeLeft;
    deadline_set(w, e->name.buf, timeLimit + 1000);

    while (*timeLeft >= 0 && !result) {
        engine_readln(w, e, &line);

        const int64_t now = system_msec();
        info->time = now - start;
        *timeLeft = timeLimit - now;

        const char *tail = NULL;

        if ((tail = str_prefix(line.buf, "info "))) {
            while ((tail = str_tok(tail, &token, " "))) {
                if (!strcmp(token.buf, "depth")) {
                    if ((tail = str_tok(tail, &token, " ")))
                        info->depth = atoi(token.buf);
                } else if (!strcmp(token.buf, "score")) {
                    if ((tail = str_tok(tail, &token, " "))) {
                        if (!strcmp(token.buf, "cp") && (tail = str_tok(tail, &token, " ")))
                            info->score = atoi(token.buf);
                        else if (!strcmp(token.buf, "mate") && (tail = str_tok(tail, &token, " "))) {
                            const int movesToMate = atoi(token.buf);
                            info->score = movesToMate < 0 ? INT_MIN - movesToMate : INT_MAX - movesToMate;
                        } else
                            DIE("illegal syntax after 'score' in '%s'\n", line.buf);
                    }
                } else if (!strcmp(token.buf, "pv"))
                    str_cpy_c(pv, tail + strspn(tail, " "));
            }
        } else if ((tail = str_prefix(line.buf, "bestmove "))) {
            str_tok(tail, &token, " ");
            str_cpy(best, token);
            result = true;
        }
    }

    // Time out. Send "stop" and give the opportunity to the engine to respond with bestmove (still
    // under deadline protection).
    if (!result) {
        engine_writeln(w, e, "stop");

        do {
            engine_readln(w, e, &line);
        } while (!str_prefix(line.buf, "bestmove "));
    }

    deadline_clear(w);
    return result;
}

static bool xboard_engine_bestmove(Worker *w, const Engine *e, int64_t *timeLeft, str_t *best, str_t *pv,
    Info *info)
{
    int result = false;
    scope(str_destroy) str_t line = str_init(), token = str_init();
    str_clear(pv);

    const int64_t start = system_msec(), timeLimit = start + *timeLeft;
    deadline_set(w, e->name.buf, timeLimit + 1000);

    while (*timeLeft >= 0 && !result) {
        engine_readln(w, e, &line);

        const int64_t now = system_msec();
        info->time = now - start;
        *timeLeft = timeLimit - now;

        const char *tail = NULL;
        const char *msg = line.buf;
        char buf1[line.len+1];
        char buf2[line.len+1];
        char move[line.len+1];

        /* We expect 2 types of messages:
         * - move command. The format is supposed to be "move e2e4" but apparently
         *   there is an (older ?) undocumented version of the protocol which is used
         *   by GNUchess and goes "1. ... e2e4"
         * - engine thinking. Format is "<depth> <score> <time> <nodes> <pv>"
         */
        if ((sscanf(msg, "%s %s %s", buf1, buf2, move) == 3 && strcmp(buf2, "...") == 0) ||
                (sscanf(msg, "%s %s", buf1, move) == 2 && strcmp(buf1, "move") == 0)) {
            str_cpy_c(best, move);
            if (w->log)
                fprintf(w->log, "Got move %s\n", best->buf);
            result = true;
        } else if (sscanf(msg, "%d %d %*d %*d %s", &info->depth, &info->score, buf1) == 3) {
            str_cpy_c(pv, buf1);
            if (w->log)
                fprintf(w->log, "Got thinking depth %d score %d pv %s\n", info->depth, info->score, pv->buf);
        }
    }

    // Time out. Send "stop" and give the opportunity to the engine to respond with bestmove (still
    // under deadline protection).
	// TODO ? Not sure that's possible with xboard

    deadline_clear(w);
    return result;
}

bool engine_bestmove(Worker *w, const Engine *e, int64_t *timeLeft, str_t *best, str_t *pv,
    Info *info)
{
    if (e->proto == PROTO_UCI)
        return uci_engine_bestmove(w, e, timeLeft, best, pv, info);
    else
        return xboard_engine_bestmove(w, e, timeLeft, best, pv, info);
}
