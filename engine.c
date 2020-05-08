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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "engine.h"
#include "util.h"

static void engine_spawn(Engine *e, const char *cmd)
{
    // Pipe diagram: Parent -> [1]into[0] -> Child -> [1]outof[0] -> Parent
    // 'into' and 'outof' are pipes, each with 2 ends: read=0, write=1
    int outof[2], into[2];

    if (pipe(outof) < 0 || pipe(into) < 0)
        die("pipe() failed in engine_spawn()");

    e->pid = fork();

    if (e->pid == 0) {
        // in the child process
        close(into[1]);
        close(outof[0]);

        if (dup2(into[0], STDIN_FILENO) == -1)
            die("dup2() failed in engine_spawn()");
        close(into[0]);

        if (dup2(outof[1], STDOUT_FILENO) == -1)
            die("dup2() failed in engine_spawn()");
        close(outof[1]);

        if (execlp(cmd, cmd, NULL) == -1)
            die("execlp() failed in engine_spawn()");
    } else if (e->pid > 0) {
        // in the parent process
        close(into[0]);
        close(outof[1]);

        if (!(e->in = fdopen(outof[0], "r")))
            die("fdopen() failed in engine_spawn()");

        if (!(e->out = fdopen(into[1], "w")))
            die("fdopen() failed in engine_spawn()");

        // FIXME: doesn't work on Windows
        setvbuf(e->in, NULL, _IONBF, 0);
        setvbuf(e->out, NULL, _IONBF, 0);
    } else
        // fork failed
        die("fork() failed in engine_spawn()");
}

void engine_create(Engine *e, const char *cmd, FILE *log)
{
    engine_spawn(e, cmd);  // spawn child process and plug communication pipes
    e->log = log;

    e->name = str_dup(cmd); // FIXME: parse from 'id name %s'

    engine_writeln(e, "uci\n");

    str_t line = str_new();

    do {
        engine_readln(e, &line);
    } while (strcmp(line.buf, "uciok\n"));

    str_free(&line);
}

void engine_destroy(Engine *e)
{
    fclose(e->in);
    fclose(e->out);
    str_free(&e->name);

    if (kill(e->pid, SIGTERM) < 0)
        die("engine_destroy() failed");
}

void engine_readln(const Engine *e, str_t *line)
{
    if (str_getdelim(line, '\n', e->in)) {
        if (e->log && fprintf(e->log, "%s -> %s", e->name.buf, line->buf) <= 0)
            die("engine_writeln() failed writing to e->log");
    } else
        die("engine_readln() failed reading from e->in");
}

void engine_writeln(const Engine *e, char *buf)
{
    if (fputs(buf, e->out) >= 0) {
        if (e->log && fprintf(e->log, "%s <- %s", e->name.buf, buf) <= 0)
            die("engine_writeln() failed writing to e->log");
    } else
        die("engine_writeln() failed writing to e->out");
}

void engine_sync(const Engine *e)
{
    engine_writeln(e, "isready\n");

    str_t line = str_new();

    do {
        engine_readln(e, &line);
    } while (strcmp(line.buf, "readyok\n"));

    str_free(&line);
}

str_t engine_bestmove(const Engine *e)
{
    str_t best = str_dup("0000");  // default value
    str_t line = str_new(), token = str_new();

    while (true) {
        engine_readln(e, &line);
        const char *tail = str_tok(line.buf, &token, " \n");

        if (tail && !strcmp(token.buf, "bestmove")) {
            if (str_tok(tail, &token, " \n"))
                str_cpy(&best, token.buf);

            break;
        }
    }

    str_free(&line, &token);
    return best;
}
