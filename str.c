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
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "str.h"
#include "util.h"

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
    if (s->alloc < str_round_up(len + 1)) {
        s->alloc = str_round_up(len + 1);
        s->buf = realloc(s->buf, s->alloc);
    }

    s->buf[len] = '\0';
}

bool str_ok(const str_t *s)
{
    return s && ((s->alloc >= str_round_up(s->len + 1) && s->buf && s->buf[s->len] == '\0'
        && !memchr(s->buf, 0, s->len)) || (!s->alloc && !s->len && !s->buf));
}

bool str_eq(const str_t *s1, const str_t *s2)
{
    assert(str_ok(s1) && str_ok(s2));
    return s1->len == s2->len && !memcmp(s1->buf, s2->buf, s1->len);
}

str_t str_dup(const char *src)
{
    str_t s = {0};
    str_cpy(&s, src);
    return s;
}

void str_del(str_t *s)
{
    free(s->buf);
    *s = (str_t){0};
}

static str_t *do_str_cat(str_t *dest, const char *src, size_t n)
{
    assert(str_ok(dest) && src);

    const size_t oldLen = dest->len;
    str_resize(dest, oldLen + n);
    memcpy(&dest->buf[oldLen], src, n);

    assert(str_ok(dest));
    return dest;
}

str_t *str_cpy(str_t *dest, const char *restrict src)
{
    str_resize(dest, 0);
    return do_str_cat(dest, src, strlen(src));
}

str_t *str_cpy_s(str_t *dest, const str_t *src)
{
    str_resize(dest, 0);
    return do_str_cat(dest, src->buf, src->len);
}

str_t *str_ncpy(str_t *dest, const char *restrict src, size_t n)
{
    n = min(n, strlen(src));
    str_resize(dest, 0);
    return do_str_cat(dest, src, n);
}

str_t *str_push(str_t *dest, char c)
{
    str_resize(dest, dest->len + 1);
    dest->buf[dest->len - 1] = c;
    assert(str_ok(dest));
    return dest;
}

str_t *str_ncat(str_t *dest, const char *src, size_t n)
{
    n = min(n, strlen(src));
    return do_str_cat(dest, src, n);
}

str_t *str_cat(str_t *dest, const char *src)
{
    return do_str_cat(dest, src, strlen(src));
}

str_t *str_cat_s(str_t *dest, const str_t *src)
{
    return do_str_cat(dest, src->buf, src->len);
}

static char *do_fmt_u(uintmax_t n, char *s)
{
    *s-- = '\0';

    do {
        *s-- = '0' + n % 10;
    } while (n /= 10);

    return s + 1;
}

void str_cat_fmt(str_t *dest, const char *fmt, ...)
// Supported formats
// - Integers: %i (int), %I (intmax_t), %u (unsigned), %U (uintmax_t)
// - Strings: %s (const char *), %S (const str_t *)
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
            do_str_cat(dest, fmt, (size_t)(pct - fmt));

        bytesLeft -= (size_t)((pct + 2) - fmt);
        fmt = pct + 2;  // move past the '%?' to prepare next loop iteration
        assert(strlen(fmt) == bytesLeft);

        assert(sizeof(intmax_t) <= 8);
        char buf[24];  // enough to fit a intmax_t with sign prefix '-' and '\0' terminator

        if (pct[1] == 's')
            str_cat(dest, va_arg(args, const char *restrict));  // C-string
        else if (pct[1] == 'S')
            str_cat_s(dest, va_arg(args, const str_t *));  // string
        else if (pct[1] == 'i' || pct[1] == 'I') {  // int or intmax_t
            const intmax_t i = pct[1] == 'i' ? va_arg(args, int) : va_arg(args, intmax_t);
            char *s = do_fmt_u((uintmax_t)imaxabs(i), &buf[sizeof(buf) - 1]);
            if (i < 0) *--s = '-';
            str_cat(dest, s);
        } else if (pct[1] == 'u')  // unsigned int
            str_cat(dest, do_fmt_u(va_arg(args, unsigned), &buf[sizeof(buf) - 1]));
        else if (pct[1] == 'U')  // uintmax_t
            str_cat(dest, do_fmt_u(va_arg(args, uintmax_t), &buf[sizeof(buf) - 1]));
        else
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

    str_resize(token, 0);

    // eat delimiters before token
    s += strspn(s, delim);

    // eat non delimiters into token
    const size_t n = strcspn(s, delim);
    str_ncat(token, s, n);
    s += n;

    // return string tail or NULL if token empty
    assert(str_ok(token));
    return token->len ? s : NULL;
}

size_t str_getline(str_t *out, FILE *in)
{
    assert(str_ok(out) && in);
    str_resize(out, 0);
    int c;

    flockfile(in);

    while (true) {
        c = getc_unlocked(in);

        if (c != '\n' && c != EOF)
            str_push(out, c);
        else
            break;
    }

    funlockfile(in);

    const size_t n = out->len + (c == '\n');

    assert(str_ok(out));
    return n;
}
