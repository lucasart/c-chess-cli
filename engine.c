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

bool engine_start(Engine *e, const char *cmd)
{
    // Diagram: Parent -> into -> Child -> outof -> Parent
    // Each pipe have 2 ends: read end is 0, write end is 1
    int outof[2], into[2];
    #define PARENT_READ  outof[0]
    #define CHILD_WRITE  outof[1]
    #define CHILD_READ   into[0]
    #define PARENT_WRITE into[1]

    if (pipe(outof) < 0 || pipe(into) < 0)
        return false;

    e->pid = fork();

    if (e->pid == 0) {
        // in the child process
        close(PARENT_WRITE);
        close(PARENT_READ);

        if (dup2(CHILD_READ, STDIN_FILENO) == -1)
            return false;
        close(CHILD_READ);

        if (dup2(CHILD_WRITE, STDOUT_FILENO) == -1)
            return false;
        close(CHILD_WRITE);

        if (execlp(cmd, cmd, NULL) == -1)
            return false;
    } else if (e->pid > 0) {
        // in the parent process
        close(CHILD_READ);
        close(CHILD_WRITE);

        if (!(e->in = fdopen(PARENT_READ, "r")))
            return false;

        if (!(e->out = fdopen(PARENT_WRITE, "w")))
            return false;

        // FIXME: doesn't work on Windows
        setvbuf(e->in, NULL, _IONBF, 0);
        setvbuf(e->out, NULL, _IONBF, 0);
    } else
        // fork failed
        return false;

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

bool engine_readln(const Engine *e, char *buf, size_t n)
{
    if (fgets(buf, n, e->in) >= 0) {
        printf("%s -> %s", e->name, buf);
        return true;
    }

    return false;
}

bool engine_writeln(const Engine *e, char *buf)
{
    if (fputs(buf, e->out) >= 0) {
        printf("%s <- %s", e->name, buf);
        return true;
    }

    return false;
}

void engine_sync(const Engine *e)
{
    engine_writeln(e, "isready\n");

    char line[MAX_LINE_CHAR];

    while (engine_readln(e, line, sizeof line) && strcmp(line, "readyok\n"));
}

void engine_bestmove(const Engine *e, char *str)
// Parse UCI output until bestmove is found and return it
// TODO: parse info lines and return the last score (for adjudication)
{
    char line[MAX_LINE_CHAR];

    while (engine_readln(e, line, sizeof line)) {
        char *linePos = NULL, *token = strtok_r(line, " \n", &linePos);

        if (token && !strcmp(token, "bestmove")) {
            strcpy(str, strtok_r(NULL, " \n", &linePos));
            break;
        }
    }
}
