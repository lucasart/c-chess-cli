#pragma once
#include <inttypes.h>
#include <stddef.h>

uint64_t hash(const void *buf, size_t len, uint64_t seed);
uint64_t prng(uint64_t *state);
