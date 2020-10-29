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

vec_t *vec_ptr(void *v)
{
    assert(v);
    return (vec_t *)((char *)(v) - offsetof(vec_t, buf));
}

const vec_t *vec_cptr(const void *v)
{
    return (const vec_t *)((const char *)(v) - offsetof(vec_t, buf));
}

void *vec_do_init(size_t capacity, size_t esize)
{
    vec_t *p = malloc(sizeof(vec_t) + capacity * esize);
    p->capacity = capacity;
    p->size = 0;
    return (void *)p->buf;
}

size_t vec_size(const void *v)
{
    return vec_cptr(v)->size;
}

size_t vec_capacity(const void *v)
{
    return vec_cptr(v)->capacity;
}

void *vec_do_grow(void *v, size_t esize, size_t n)
{
    assert(v);

    if (n * esize < 2 * sizeof(size_t))
        // First realloc must be at least 2 machine words, at least n=1 element, and the formula
        // also gives a bonus to small esize.
        n = (2 * sizeof(size_t) + esize) / esize;
    else if (n * esize + sizeof(vec_t) < 4096)
        // As long as we occupy less than a memory page, double the size
        n += n;
    else {
        // Otherwise, multiply by 1.5 (rounding up as n must strictly increase). This reduces memory
        // waste and fragmentation.
        assert(n);
        n += (n + 1) / 2;
    }

    vec_t *p = vec_ptr(v);
    p = realloc(p, sizeof(vec_t) + esize * n);
    p->capacity = n;

    return p->buf;
}

void vec_clear(void *v)
{
    vec_ptr(v)->size = 0;
}
