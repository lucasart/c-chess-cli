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
#pragma once
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

extern _Thread_local int threadId;

#define min(x, y)                                                                                  \
    ({                                                                                             \
        typeof(x) _x = (x), _y = (y);                                                              \
        _x < _y ? _x : _y;                                                                         \
    })

#define max(x, y)                                                                                  \
    ({                                                                                             \
        typeof(x) _x = (x), _y = (y);                                                              \
        _x > _y ? _x : _y;                                                                         \
    })

#define swap(x, y)                                                                                 \
    ({                                                                                             \
        typeof(x) _x = x;                                                                          \
        x = y;                                                                                     \
        y = _x;                                                                                    \
    })

#ifdef __MINGW32__
    // Opt in to CR+LF translation for text files, and opt out for binary files. Use N to make
    // handles not inheritable.
    #define FOPEN_TEXT "tN"
    #define FOPEN_BINARY "bN"
#else
    // Set the CLOEXEC flag to prevent inheritance to child processes
    #define FOPEN_TEXT "e"
    #define FOPEN_BINARY "e"
#endif

uint64_t prng(uint64_t *state);
double prngf(uint64_t *state);

int64_t system_msec(void);
void system_sleep(int64_t msec);

#define DIE(...)                                                                                   \
    do {                                                                                           \
        fprintf(stderr, __VA_ARGS__);                                                              \
        exit(EXIT_FAILURE);                                                                        \
    } while (0)

_Noreturn void die_errno(const char *fileName, int line);

#define DIE_IF(v)                                                                                  \
    ({                                                                                             \
        if (v)                                                                                     \
            die_errno(__FILE__, __LINE__);                                                         \
    })

void stdio_lock(FILE *f);
void stdio_unlock(FILE *f);
