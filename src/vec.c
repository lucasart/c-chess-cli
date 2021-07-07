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
#include "vec.h"

vec_t *vec_ptr(void *v) {
    assert(v);
    return (vec_t *)((size_t *)(v)-offsetof(vec_t, buf) / sizeof(size_t));
}

const vec_t *vec_cptr(const void *v) {
    return (const vec_t *)((const size_t *)(v)-offsetof(vec_t, buf) / sizeof(size_t));
}

void *vec_do_init(size_t capacity, size_t esize) {
    vec_t *p = malloc(sizeof(vec_t) + capacity * esize);
    p->capacity = capacity;
    p->size = 0;
    return (void *)p->buf;
}

size_t vec_size(const void *v) { return v ? vec_cptr(v)->size : 0; }

size_t vec_capacity(const void *v) { return vec_cptr(v)->capacity; }

void *vec_do_grow(void *v, size_t esize, size_t additional) {
    vec_t *p = vec_ptr(v);
    size_t n = p->capacity;

    while (n < p->size + additional) {
        if (n * esize < 2 * sizeof(size_t)) {
            // First realloc must be at least 2 machine words
            n = (2 * sizeof(size_t) + esize) / esize;
            assert(n > 0);
        } else if (n * esize + sizeof(vec_t) < 4096) {
            // As long as we occupy less than a memory page, double the size
            assert(n > 0);
            n *= 2;
        } else {
            // Otherwise, multiply by 1.5 (rounding up to always increase)
            assert(n > 0);
            n += (n + 1) / 2;
        }
    }

    if (n > p->capacity) {
        p = realloc(p, sizeof(vec_t) + esize * n);
        p->capacity = n;
    }

    return p->buf;
}

void vec_clear(void *v) { vec_ptr(v)->size = 0; }
