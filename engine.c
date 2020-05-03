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
#include <sys/wait.h>
#include <unistd.h>
#include "engine.h"

bool engine_start(Engine *e, const char *cmd, FILE *log)
{
    // Pipe diagram: Parent -> [1]into[0] -> Child -> [1]outof[0] -> Parent
    // 'into' and 'outof' are pipes, each with 2 ends: read=0, write=1
    int outof[2], into[2];

    if (pipe(outof) < 0 || pipe(into) < 0)
        return false;

    e->pid = fork();

    if (e->pid == 0) {
        // in the child process
        close(into[1]);
        close(outof[0]);

        if (dup2(into[0], STDIN_FILENO) == -1)
            return false;
        close(into[0]);

        if (dup2(outof[1], STDOUT_FILENO) == -1)
            return false;
        close(outof[1]);

        if (execlp(cmd, cmd, NULL) == -1)
            return false;
    } else if (e->pid > 0) {
        // in the parent process
        close(into[0]);
        close(outof[1]);

        if (!(e->in = fdopen(outof[0], "r")))
            return false;

        if (!(e->out = fdopen(into[1], "w")))
            return false;

        // FIXME: doesn't work on Windows
        setvbuf(e->in, NULL, _IONBF, 0);
        setvbuf(e->out, NULL, _IONBF, 0);
    } else
        // fork failed
        return false;

    e->log = log;
    return true;
}

bool engine_stop(Engine *e)
{
    // close the parent side of the pipes
    fclose(e->in);
    fclose(e->out);

    // terminate the child process
    return kill(e->pid, SIGTERM) >= 0;
}

bool engine_readln(const Engine *e, str_t *line)
{
    if (str_getdelim(line, '\n', e->in)) {
        if (e->log)
            fprintf(e->log, "%s -> %s", e->name, line->buf);

        return true;
    }

    return false;
}

bool engine_writeln(const Engine *e, char *buf)
{
    if (fputs(buf, e->out) >= 0) {
        if (e->log)
            fprintf(e->log, "%s <- %s", e->name, buf);

        return true;
    }

    return false;
}

void engine_sync(const Engine *e)
{
    engine_writeln(e, "isready\n");

    str_t line = str_new();

    while (engine_readln(e, &line) && strcmp(line.buf, "readyok\n"));

    str_free(&line);
}

void engine_bestmove(const Engine *e, char *str)
{
    strcpy(str, "0000");  // default value
    str_t line = str_new();

    while (engine_readln(e, &line)) {
        char *linePos = NULL, *token = strtok_r(line.buf, " \n", &linePos);

        if (token && !strcmp(token, "bestmove")) {
            token = strtok_r(NULL, " \n", &linePos);

            if (token)
                strcpy(str, token);

            break;
        }
    }

    str_free(&line);
}
