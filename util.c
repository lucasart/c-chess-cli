#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "util.h"

// SplitMix64 PRNG, based on http://xoroshiro.di.unimi.it/splitmix64.c
uint64_t prng(uint64_t *state)
{
    uint64_t rnd = (*state += 0x9E3779B97F4A7C15);
    rnd = (rnd ^ (rnd >> 30)) * 0xBF58476D1CE4E5B9;
    rnd = (rnd ^ (rnd >> 27)) * 0x94D049BB133111EB;
    rnd ^= rnd >> 31;
    return rnd;
}

// Simple hash function I derived from SplitMix64. Known limitations:
// - alignment: 'buffer' must be 8-byte aligned.
// - length: must be a multiple of 8 bytes.
// - endianness: don't care (assume little-endian).
uint64_t hash(const void *buffer, size_t length, uint64_t seed)
{
    assert((uintptr_t)buffer % 8 == 0 && length % 8 == 0);
    const uint64_t *blocks = (const uint64_t *)buffer;
    uint64_t result = 0;

    for (size_t i = 0; i < length / 8; i++) {
        seed ^= blocks[i];
        result ^= prng(&seed);
    }

    return result;
}

void die(const char *msg)
{
    perror(msg);
    exit(1);
}

int64_t system_msec()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}
