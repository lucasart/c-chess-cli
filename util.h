#pragma once
#include <inttypes.h>
#include <stddef.h>

uint64_t prng(uint64_t *state);
uint64_t hash(const void *buffer, size_t length, uint64_t seed);

int64_t system_msec();

void die(const char *fmt, ...);
