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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "str.h"

static size_t str_round_up(size_t n)
// Round to the next power of 2, at least the size of 2 machine words
{
    size_t p2 = 2 * sizeof(size_t);

    while (p2 < n)
        p2 *= 2;

    return p2;
}

static void str_resize(str_t *s, size_t len)
// After this call, 's' may be invalid according to the strict definition of str_ok(), because it
// may contain 0's before the end. This will cause problems with most code, starting with the C
// standard library, which is why this function is not exposed to the API.
{
    assert(str_ok(s));
    s->len = len;

    // Implement lazy realloc strategy
    if (s->alloc != str_round_up(len + 1)) {
        s->alloc = str_round_up(len + 1);
        s->buf = realloc(s->buf, s->alloc);
        countRealloc++;
    }

    s->buf[len] = '\0';
}

bool str_ok(const str_t *s)
{
    return s && s->alloc == str_round_up(s->len + 1)
        && s->buf && s->buf[s->len] == '\0' && !memchr(s->buf, 0, s->len);
}

str_t str_new()
{
    str_t s;

    s.len = 0;
    s.alloc = str_round_up(s.len + 1);
    s.buf = malloc(s.alloc);
    s.buf[s.len] = '\0';

    assert(str_ok(&s));
    return s;
}

str_t str_dup(const char *src)
{
    assert(src);
    str_t s;

    s.len = strlen(src);
    s.alloc = str_round_up(s.len + 1);
    s.buf = malloc(s.alloc);
    strcpy(s.buf, src);

    assert(str_ok(&s));
    return s;
}

void str_free_aux(str_t *s1, ...)
{
    va_list args;
    va_start(args, s1);
    str_t *next = s1;

    while (next) {
        assert(str_ok(next));
        free(next->buf);
        *next = (str_t){0};

        next = va_arg(args, str_t *);
    }

    va_end(args);
}

void str_cpy(str_t *dest, const char *src)
{
    assert(str_ok(dest) && src);

    str_resize(dest, strlen(src));
    memcpy(dest->buf, src, dest->len + 1);

    assert(str_ok(dest));
}

void str_putc_aux(str_t *dest, int c1, ...)
{
    assert(str_ok(dest));
    va_list args;
    va_start(args, c1);
    int next = c1;

    while (next) {
        str_resize(dest, dest->len + 1);
        dest->buf[dest->len - 1] = (char)next;

        next = va_arg(args, int);
    }

    va_end(args);
    assert(str_ok(dest));
}

void str_ncat(str_t *dest, const char *src, size_t n)
{
    assert(str_ok(dest));

    if (strlen(src) < n)
        n = strlen(src);

    const size_t oldLen = dest->len;
    str_resize(dest, dest->len + n);
    memcpy(&dest->buf[oldLen], src, n);

    assert(str_ok(dest));
}

void str_cat_aux(str_t *dest, const char *s1, ...)
{
    assert(str_ok(dest));
    va_list args;
    va_start(args, s1);
    const char *next = s1;

    while (next) {
        const size_t initial = dest->len, additional = strlen(next);
        str_resize(dest, initial + additional);
        memcpy(&dest->buf[initial], next, additional + 1);
        assert(str_ok(dest));

        next = va_arg(args, const char *);
    }

    va_end(args);
    assert(str_ok(dest));
}

void str_catf(str_t *dest, const char *fmt, ...)
{
    assert(str_ok(dest) && fmt);

    va_list args;
    va_start(args, fmt);

    size_t bytesLeft = strlen(fmt);

    while (bytesLeft) {
        const char *pct = memchr(fmt, '%', bytesLeft);

        if (!pct) {
            // '%' not found: append the rest of the format string and we're done
            str_cat(dest, fmt);
            break;
        }

        assert(pct >= fmt && *pct == '%');

        if (pct > fmt)
            // '%' found: append the chunk of format string before '%' (if any)
            str_ncat(dest, fmt, pct - fmt);

        bytesLeft -= (pct + 2) - fmt;
        fmt = pct + 2;  // move past the '%?' to prepare next loop iteration
        assert(strlen(fmt) == bytesLeft);

        if (pct[1] == 's')
            str_cat(dest, va_arg(args, const char *));
        else if (pct[1] == 'd') {
            char buf[16];
            sprintf(buf, "%d", va_arg(args, int));
            str_cat(dest, buf);
        } else
            assert(false);  // add your format specifier handler here
    }

    va_end(args);
    assert(str_ok(dest));
}

const char *str_tok(const char *s, str_t *token, const char *delim)
{
    assert(str_ok(token) && delim && *delim);

    // empty tail: no-op
    if (!s)
        return NULL;

    char c;
    str_cpy(token, "");
    const size_t n = strlen(delim);

    // eat delimiters before token
    while ((c = *s)) {
        if (memchr(delim, c, n))
            s++;
        else
            break;
    }

    // eat non delimiters into token
    while ((c = *s)) {
        if (!memchr(delim, c, n)) {
            str_putc(token, c);
            s++;
         } else
             break;
    }

    // return string tail or NULL if token empty
    assert(str_ok(token));
    return token->len ? s : NULL;
}

size_t str_getline(str_t *out, FILE *in)
{
    assert(str_ok(out) && in);
    str_cpy(out, "");
    int c;

    flockfile(in);

    do {
        c = getc_unlocked(in);

        if (c != EOF)
            str_putc(out, c);
    } while (c != '\n' && c != EOF);

    funlockfile(in);

    assert(str_ok(out));
    return out->len;
}
