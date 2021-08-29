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
#ifdef __MINGW32__
    #include <direct.h>
    #include <fcntl.h>
    #include <io.h>
#elif defined __linux__
    #define _GNU_SOURCE
    #include <fcntl.h>
    #include <sys/prctl.h>
    #include <sys/wait.h>
    #include <unistd.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "util.h"
#include "vec.h"

#ifdef __MINGW32__
// Spawning sub-processes must be sequential on Windows, for (at least) 2 reasons:
// (1) handle inheritance bug as CreatePipe() and stuff are racy
// (2) chdir is done in the context of the parent process (not child after fork like on POSIX)
static pthread_mutex_t mtxSequentialSpawn = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor)) static void associate_job(void) {
    // Setup a Windows job and associate the parent process to it. Note that all children will
    // automatically be associated to this job.
    HANDLE hJob = CreateJobObject(NULL, NULL);
    DIE_IF(!hJob);
    JOBOBJECT_BASIC_LIMIT_INFORMATION jobBasicInfo = {.LimitFlags =
                                                          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobExtendedInfo = {.BasicLimitInformation = jobBasicInfo};
    DIE_IF(!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobExtendedInfo,
                                    sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)));
    DIE_IF(!AssignProcessToJobObject(hJob, GetCurrentProcess()));
}
#endif

static void engine_spawn(Engine *e, const char *cwd, char **argv, bool readStdErr) {
    assert(argv[0]);

#ifdef __MINGW32__ // Windows (mingw only)
    (void)argv;    // FIXME: support engine arguments

    SECURITY_ATTRIBUTES saAttr = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL,
    };

    // Pipe handler: read=0, write=1
    HANDLE into[2] = {0}, outof[2] = {0};

    // Recompose cmdFromCwd = "argv[0] argv[1] ... argv[argc-1]", which is the command to execute
    // from the context of cwd. This may differ from cmd in engine_init(), because argv[0] strips
    // out path specification (which goes into cwd).
    scope(str_destroy) str_t cmdFromCwd = str_init_from_c(argv[0]);

    for (int i = 1; argv[i]; i++)
        str_cat_fmt(&cmdFromCwd, " %s", argv[i]);

    pthread_mutex_lock(&mtxSequentialSpawn);

    // Create a pipe for child process's STDOUT
    DIE_IF(!CreatePipe(&outof[0], &outof[1], &saAttr, 0));
    DIE_IF(!SetHandleInformation(outof[0], HANDLE_FLAG_INHERIT, 0));

    // Create a pipe for child process's STDIN
    DIE_IF(!CreatePipe(&into[0], &into[1], &saAttr, 0));
    DIE_IF(!SetHandleInformation(into[1], HANDLE_FLAG_INHERIT, 0));

    // Create the child process
    PROCESS_INFORMATION piProcInfo = {0};
    STARTUPINFO siStartInfo = {.cb = sizeof(STARTUPINFO),
                               .hStdOutput = outof[1],
                               .hStdInput = into[0],
                               .dwFlags = STARTF_USESTDHANDLES};

    if (readStdErr) {
        HANDLE hStdError = 0;
        DIE_IF(!DuplicateHandle(GetCurrentProcess(), outof[1], GetCurrentProcess(), &hStdError, 0,
                                TRUE, DUPLICATE_SAME_ACCESS));
        siStartInfo.hStdError = hStdError;
    }

    // Launch engine process
    DIE_IF(_chdir(cwd) < 0);
    DIE_IF(!CreateProcessA(NULL, cmdFromCwd.buf, NULL, NULL, TRUE,
                           CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, NULL, cwd, &siStartInfo,
                           &piProcInfo));

    // Close handles to the stdin and stdout pipes no longer needed
    DIE_IF(!CloseHandle(into[0]));
    DIE_IF(!CloseHandle(outof[1]));

    pthread_mutex_unlock(&mtxSequentialSpawn);

    // Keep the handle to the child process
    e->hProcess = piProcInfo.hProcess;

    // Close the handle to the child's primary thread
    DIE_IF(!CloseHandle(piProcInfo.hThread));

    // Reopen stdin and stdout pipes using C style FILE
    int stdin_fd = _open_osfhandle((intptr_t)into[1], _O_WRONLY | _O_TEXT);
    int stdout_fd = _open_osfhandle((intptr_t)outof[0], _O_RDONLY | _O_TEXT);
    DIE_IF(stdin_fd == -1);
    DIE_IF(stdout_fd == -1);
    DIE_IF(!(e->in = _fdopen(stdout_fd, "r")));
    DIE_IF(!(e->out = _fdopen(stdin_fd, "w")));

#else // POSIX: Linux/Android == __linux__, otherwise assume __APPLE__
    // Pipe diagram: Parent -> [1]into[0] -> Child -> [1]outof[0] -> Parent
    // 'into' and 'outof' are pipes, each with 2 ends: read=0, write=1
    int outof[2] = {0}, into[2] = {0};

    #ifdef __linux__
    DIE_IF(pipe2(outof, O_CLOEXEC) < 0);
    DIE_IF(pipe2(into, O_CLOEXEC) < 0);
    #else
    DIE_IF(pipe(outof) < 0);
    DIE_IF(pipe(into) < 0);
    #endif

    DIE_IF((e->pid = fork()) < 0);

    if (e->pid == 0) {
    #ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGHUP); // delegate zombie purge to the kernel
    #endif
        // Plug stdin and stdout
        DIE_IF(dup2(into[0], STDIN_FILENO) < 0);
        DIE_IF(dup2(outof[1], STDOUT_FILENO) < 0);

        // For stderr we have 2 choices:
        // - readStdErr=true: dump it into stdout, like doing '2>&1' in bash. This is useful, if we
        // want to see error messages from engines in their respective log file (notably assert()
        // writes to stderr). Of course, such error messages should not be UCI commands, otherwise
        // we will be fooled into parsing them as such.
        // - readStdErr=false: do nothing, which means stderr is inherited from the parent process.
        // Typcically, this means all engines write their error messages to the terminal (unless
        // redirected otherwise).
        if (readStdErr)
            DIE_IF(dup2(outof[1], STDERR_FILENO) < 0);

    #ifndef __linux__
        // Ugly (and slow) workaround for Apple's BSD-based kernels that lack the ability to
        // atomically set O_CLOEXEC when creating pipes.
        for (int fd = 3; fd < sysconf(FOPEN_MAX); close(fd++))
            ;
    #endif

        // Set cwd as current directory, and execute run with argv[]
        DIE_IF(chdir(cwd) < 0);
        DIE_IF(execvp(argv[0], argv) < 0);
    } else {
        assert(e->pid > 0);

        // in the parent process
        DIE_IF(close(into[0]) < 0);
        DIE_IF(close(outof[1]) < 0);

        DIE_IF(!(e->in = fdopen(outof[0], "r")));
        DIE_IF(!(e->out = fdopen(into[1], "w")));
    }
#endif
}

#ifdef __MINGW32__
    // Use Windows backslash for file system, and PowerShell's '`' as escape sequence
    #define CUR_DIR ".\\"
    #define DIR_SEP '\\'
    #define ESC_SEQ '`'
#else
    // Use POSIX forward slash for file system, and Bash '\\' as escape sequence
    #define CUR_DIR "./"
    #define DIR_SEP '/'
    #define ESC_SEQ '\\'
#endif

static str_t *engine_parse_cmd(const char *cmd, str_t *cwd, str_t *run) {
    str_t *vecArgs = vec_init(str_t);
    scope(str_destroy) str_t token = str_init();

    // Isolate the first token being the command to run.
    const char *tail = cmd;
    tail = str_tok_esc(tail, &token, ' ', ESC_SEQ);

    // Split token into (cwd, run). Possible cases:
    // (a) unqualified path, like "demolito" (which evecvp() will search in PATH)
    // (b) qualified path (absolute starting with "/", or relative starting with "./" or "../")
    // For (b), we want to separate into executable and directory, so instead of running
    // "../Engines/demolito" from the cwd, we execute run="./demolito" from cwd="../Engines"
    str_cpy_c(cwd, CUR_DIR);
    str_cpy(run, token);
    const char *lastSlash = strrchr(token.buf, DIR_SEP);

    if (lastSlash) {
        str_ncpy(cwd, token, (size_t)(lastSlash - token.buf));
        str_cpy_fmt(run, CUR_DIR "%s", lastSlash + 1);
    }

    // Collect the arguments into a vec of str_t, vecArgs[]
    vec_push(vecArgs, str_init_from(*run)); // argv[0] is the executed command

    while ((tail = str_tok_esc(tail, &token, ' ', ESC_SEQ)))
        vec_push(vecArgs, str_init_from(token));

    return vecArgs;
}

Engine engine_init(Worker *w, const char *cmd, const char *name, const str_t *options,
                   int64_t timeOut) {
    if (!*cmd)
        DIE("[%d] missing command to start engine.\n", threadId);

    Engine e = {.name = str_init_from_c(*name ? name : cmd), // default value
                .timeOut = timeOut};

    // Parse cmd into (cwd, run, vecArgs): we want to execute run from cwd with vecArgs.
    scope(str_destroy) str_t cwd = str_init(), run = str_init();
    str_t *vecArgs = engine_parse_cmd(cmd, &cwd, &run);

    // execvp() needs NULL terminated char **, not vec of str_t. Prepare a char **, whose elements
    // point to the C-string buffers of the elements of vecArgs, with the required NULL at the end.
    char **argv = calloc(vec_size(vecArgs) + 1, sizeof(char *));

    for (size_t i = 0; i < vec_size(vecArgs); i++)
        argv[i] = vecArgs[i].buf;

    // Spawn child process and plug pipes
    engine_spawn(&e, cwd.buf, argv, w->log != NULL);

    vec_destroy_rec(vecArgs, str_destroy);
    free(argv);

    // Start the uci..uciok dialogue
    deadline_set(w, e.name.buf, system_msec(), e.timeOut);
    engine_writeln(w, &e, "uci");
    scope(str_destroy) str_t line = str_init();

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
        const char *tail = NULL;

        if ((tail = str_tok_esc(options[i].buf, &oname, '=', ESC_SEQ)) &&
            (tail = str_tok_esc(tail, &ovalue, '=', ESC_SEQ))) {
            str_cpy_fmt(&line, "setoption name %S value %S", oname, ovalue);

            deadline_set(w, e.name.buf, system_msec(), e.timeOut);
            engine_writeln(w, &e, line.buf);
            deadline_clear(w);
        } else
            DIE("Cannot parse '%s'\n", options[i].buf);
    }

    return e;
}

void engine_destroy(Worker *w, Engine *e) {
    // Engine was not instanciated with engine_init()
    if (!e->in)
        return;

    // Order the engine to quit, and grant 1s deadline for obeying
    deadline_set(w, e->name.buf, system_msec(), e->timeOut);
    engine_writeln(w, e, "quit");

#ifdef __MINGW32__
    WaitForSingleObject(e->hProcess, INFINITE);
    CloseHandle(e->hProcess);
#else
    waitpid(e->pid, NULL, 0);
#endif

    deadline_clear(w);

    str_destroy(&e->name);
    DIE_IF(fclose(e->in) < 0);
    DIE_IF(fclose(e->out) < 0);
}

void engine_readln(const Worker *w, const Engine *e, str_t *line) {
    if (!str_getline(line, e->in))
        DIE("[%d] could not read from %s\n", threadId, e->name.buf);

    if (w->log)
        DIE_IF(fprintf(w->log, "%s -> %s\n", e->name.buf, line->buf) < 0);
}

void engine_writeln(const Worker *w, const Engine *e, char *buf) {
    DIE_IF(fputs(buf, e->out) < 0);
    DIE_IF(fputc('\n', e->out) < 0);
    DIE_IF(fflush(e->out) < 0);

    if (w->log) {
        DIE_IF(fprintf(w->log, "%s <- %s\n", e->name.buf, buf) < 0);
        DIE_IF(fflush(w->log) < 0);
    }
}

void engine_newgame(Worker *w, const Engine *e) {
    deadline_set(w, e->name.buf, system_msec(), e->timeOut);
    engine_writeln(w, e, "ucinewgame");
    deadline_clear(w);
}

void engine_sync(Worker *w, const Engine *e) {
    deadline_set(w, e->name.buf, system_msec(), e->timeOut);
    engine_writeln(w, e, "isready");
    scope(str_destroy) str_t line = str_init();

    do {
        engine_readln(w, e, &line);
    } while (strcmp(line.buf, "readyok"));

    deadline_clear(w);
}

static void engine_parse_info(const char *tail, Info *info, str_t *pv) {
    scope(str_destroy) str_t token = str_init();

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
                    info->score =
                        movesToMate < 0 ? INT16_MIN - movesToMate : INT16_MAX - movesToMate;
                } else
                    DIE("%s(): illegal syntax after 'score' here '%s'\n", __func__, tail);
            }
        } else if (!strcmp(token.buf, "pv"))
            // FIXME: pv really belongs in Info. This means Info must have ctor/dtor, of course. But
            // the benefit is that we can print the PV in the PGN afterwards.
            str_cpy_c(pv, tail + strspn(tail, " "));
    }
}

bool engine_bestmove(Worker *w, const Engine *e, int64_t *timeLeft, str_t *best, str_t *pv,
                     Info *info) {
    int result = false;
    scope(str_destroy) str_t line = str_init();
    str_clear(pv);

    const int64_t start = system_msec(), timeLimit = start + *timeLeft;
    deadline_set(w, e->name.buf, start, *timeLeft + e->timeOut);

    while (*timeLeft >= 0 && !result) {
        engine_readln(w, e, &line);

        const int64_t now = system_msec();
        info->time = now - start;
        *timeLeft = timeLimit - now;

        const char *tail = NULL;

        if ((tail = str_prefix(line.buf, "info ")))
            engine_parse_info(tail, info, pv);
        else if ((tail = str_prefix(line.buf, "bestmove "))) {
            scope(str_destroy) str_t token = str_init();
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
