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
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

_Thread_local int threadId = 0;

// SplitMix64 PRNG, based on http://xoroshiro.di.unimi.it/splitmix64.c
uint64_t prng(uint64_t *state) {
    uint64_t rnd = (*state += 0x9E3779B97F4A7C15);
    rnd = (rnd ^ (rnd >> 30)) * 0xBF58476D1CE4E5B9;
    rnd = (rnd ^ (rnd >> 27)) * 0x94D049BB133111EB;
    rnd ^= rnd >> 31;
    return rnd;
}

double prngf(uint64_t *state) { return (prng(state) >> 11) * 0x1.0p-53; }

int64_t system_msec() {
    struct timespec t = {0};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

void system_sleep(int64_t msec) {
    const struct timespec t = {.tv_sec = msec / 1000, .tv_nsec = (msec % 1000) * 1000000LL};
    nanosleep(&t, NULL);
}

_Noreturn void die_errno(const char *fileName, int line) {
    stdio_lock(stdout); // lock stderr (fprintf) and stdout (explicitely), to prevent interleaving
    fprintf(stderr, "[%d] error in %s: (%d). %s\n", threadId, fileName, line, strerror(errno));
    stdio_unlock(stdout);
    exit(EXIT_FAILURE);
}

void stdio_lock(FILE *f) {
#ifdef __MINGW32__
    _lock_file(f);
#else
    flockfile(f);
#endif
}

void stdio_unlock(FILE *f) {
#ifdef __MINGW32__
    _unlock_file(f);
#else
    funlockfile(f);
#endif
}
