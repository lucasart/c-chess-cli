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
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "str.h"

static size_t str_round_up(size_t n)
// Round to the next power of 2, which is at least the size of a machine word
{
    size_t p2 = sizeof(size_t);

    while (p2 < n)
        p2 *= 2;

    return p2;
}

static bool str_ok(const str_t *s)
{
    return s->alloc == str_round_up(s->len + 1)
        && s->buf && s->buf[s->len] == '\0';
}

void str_init(str_t *s)
{
    s->len = 0;
    s->alloc = str_round_up(1);
    s->buf = malloc(s->alloc);
    s->buf[0] = '\0';
    assert(str_ok(s));
}

void str_free(str_t *s)
{
    s->alloc = s->len = 0;
    free(s->buf), s->buf = NULL;
}

void str_resize(str_t *s, size_t len)
{
    assert(str_ok(s));
    s->len = len;

    // Implement lazy realloc strategy
    if (s->alloc != str_round_up(len + 1)) {
        s->alloc = str_round_up(len + 1);
        s->buf = realloc(s->buf, s->alloc);
    }

    s->buf[len] = '\0';
}

str_t *str_cpy(str_t *dest, const char *restrict src)
{
    assert(str_ok(dest) && src);

    str_resize(dest, strlen(src));
    memcpy(dest->buf, src, dest->len + 1);
    return dest;
}

str_t *str_cat(str_t *dest, const char *restrict src)
{
    assert(str_ok(dest) && src);
    const size_t initial = dest->len, additional = strlen(src);

    str_resize(dest, initial + additional);
    memcpy(&dest->buf[initial], src, additional + 1);
    return dest;
}
