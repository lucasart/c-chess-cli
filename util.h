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
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(x, y) ({ \
    typeof(x) _x = (x), _y = (y); \
    _x < _y ? _x : _y; \
})

#define max(x, y) ({ \
    typeof(x) _x = (x), _y = (y); \
    _x > _y ? _x : _y; \
})

uint64_t prng(uint64_t *state);
uint64_t hash(const void *buffer, size_t length, uint64_t seed);

int64_t system_msec(void);
void system_sleep(int64_t msec);

#define DIE(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    exit(EXIT_FAILURE); \
} while (0)

#define DIE_IF(id, v) do { \
    if (v) { \
        fprintf(stderr, "[%d] error in %s: (%d). %s\n", id, __FILE__, __LINE__, strerror(errno)); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
