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
#include "str.h"
#include "util.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static size_t str_round_up(size_t n)
// Round to the next power of 2, at least the size of 2 machine words
{
    if (n < 2 * sizeof(size_t))
        return 2 * sizeof(size_t);
    else if (n < 4096)
        // growth rate 2
        return n & (n - 1) ? 1ULL << (64 - __builtin_clzll(n)) // next power of 2
                           : n;                                // already a power of 2
    else {
        // growth rate 1.5
        size_t p = 4096;

        while (p < n)
            p += p / 2;

        return p;
    }
}

static void str_resize(str_t *s, size_t len)
// This function is not exposed to the API, because it does not respect the str_ok() criteria:
// - entry: may be called with (str_t){0} or a valid string.
// - exit: will not satisfy the condition !memchr(s->buf, 0, s->len), in the case of extending len
//   with early '\0' between buf[0] and buf[len-1].
{
    assert((!s->alloc && !s->len && !s->buf) || str_ok(*s));

    // Implement lazy realloc strategy
    if (s->alloc < str_round_up(len + 1)) {
        s->alloc = str_round_up(len + 1);
        s->buf = realloc(s->buf, s->alloc);
    }

    s->len = len;
    s->buf[len] = '\0';

    assert(s->alloc > s->len && s->buf && s->buf[s->len] == '\0');
}

void str_clear(str_t *s) { str_resize(s, 0); }

bool str_ok(const str_t s) {
    return s.alloc > s.len && s.buf && s.buf[s.len] == '\0' && !memchr(s.buf, 0, s.len);
}

bool str_eq(const str_t s1, const str_t s2) {
    assert(str_ok(s1) && str_ok(s2));
    return s1.len == s2.len && !memcmp(s1.buf, s2.buf, s1.len);
}

str_t str_ref(const char *src) {
    const size_t len = strlen(src);
    return (str_t){.len = len, .alloc = len + 1, .buf = (char *)src};
}

static str_t *do_str_cat(str_t *dest, const char *src, size_t n) {
    assert(str_ok(*dest) && src);

    const size_t oldLen = dest->len;
    str_resize(dest, oldLen + n);
    memcpy(&dest->buf[oldLen], src, n);

    assert(str_ok(*dest));
    return dest;
}

str_t str_init(void) {
    str_t s = {0};
    str_resize(&s, 0);
    return s;
}

str_t str_init_from(const str_t src) {
    str_t s = {0};
    str_resize(&s, src.len);
    memcpy(s.buf, src.buf, s.len);
    assert(str_ok(s));
    return s;
}

void str_destroy(str_t *s) {
    free(s->buf);
    s->buf = NULL;
}

str_t *str_cpy(str_t *dest, const str_t src) {
    str_resize(dest, 0);
    return do_str_cat(dest, src.buf, src.len);
}

str_t *str_ncpy(str_t *dest, const str_t src, size_t n) {
    n = min(n, src.len);
    str_resize(dest, 0);
    return do_str_cat(dest, src.buf, n);
}

str_t *str_push(str_t *dest, char c) {
    str_resize(dest, dest->len + 1);
    dest->buf[dest->len - 1] = c;
    assert(str_ok(*dest));
    return dest;
}

str_t *str_ncat(str_t *dest, const str_t src, size_t n) {
    n = min(n, src.len);
    return do_str_cat(dest, src.buf, n);
}

str_t *str_cat(str_t *dest, const str_t src) { return do_str_cat(dest, src.buf, src.len); }

static char *do_fmt_u(uintmax_t n, char *s) {
    *s-- = '\0';

    do {
        *s-- = '0' + n % 10;
    } while (n /= 10);

    return s + 1;
}

str_t *str_cat_int(str_t *dest, intmax_t i) {
    char buf[40]; // enough for sizeof(uintmax_t) <= 16
    char *s = do_fmt_u((uintmax_t)imaxabs(i), &buf[sizeof(buf) - 1]);

    if (i < 0)
        *--s = '-';

    return str_cat_c(dest, s);
}

str_t *str_cat_uint(str_t *dest, uintmax_t u) {
    char buf[40]; // enough for sizeof(uintmax_t) <= 16
    return str_cat_c(dest, do_fmt_u(u, &buf[sizeof(buf) - 1]));
}

static void do_str_cat_fmt(str_t *dest, const char *fmt, va_list args)
// Supported formats
// - Integers: %i (int), %I (intmax_t), %u (unsigned), %U (uintmax_t)
// - Strings: %s (const char *), %S (str_t)
{
    assert(str_ok(*dest) && fmt);

    size_t bytesLeft = strlen(fmt);

    while (bytesLeft) {
        const char *pct = memchr(fmt, '%', bytesLeft);

        if (!pct) {
            // '%' not found: append the rest of the format string and we're done
            do_str_cat(dest, fmt, bytesLeft);
            break;
        }

        assert(pct >= fmt && *pct == '%');

        if (pct > fmt)
            // '%' found: append the chunk of format string before '%' (if any)
            do_str_cat(dest, fmt, (size_t)(pct - fmt));

        bytesLeft -= (size_t)((pct + 2) - fmt);
        fmt = pct + 2; // move past the '%?' to prepare next loop iteration
        assert(strlen(fmt) == bytesLeft);

        if (pct[1] == 's')
            str_cat_c(dest, va_arg(args, const char *restrict));
        else if (pct[1] == 'S')
            str_cat(dest, va_arg(args, str_t));
        else if (pct[1] == 'i')
            str_cat_int(dest, va_arg(args, int));
        else if (pct[1] == 'I')
            str_cat_int(dest, va_arg(args, intmax_t));
        else if (pct[1] == 'u')
            str_cat_uint(dest, va_arg(args, unsigned));
        else if (pct[1] == 'U')
            str_cat_uint(dest, va_arg(args, uintmax_t));
        else
            assert(false); // add your format specifier handler here
    }

    assert(str_ok(*dest));
}

str_t *str_cpy_fmt(str_t *dest, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_resize(dest, 0);
    do_str_cat_fmt(dest, fmt, args);
    va_end(args);
    return dest;
}

str_t *str_cat_fmt(str_t *dest, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    do_str_cat_fmt(dest, fmt, args);
    va_end(args);
    return dest;
}

const char *str_tok(const char *s, str_t *token, const char *delim) {
    assert(str_ok(*token) && delim && *delim);

    // empty tail: no-op
    if (!s)
        return NULL;

    str_resize(token, 0);

    // eat delimiters before token
    s += strspn(s, delim);

    // eat non delimiters into token
    const size_t n = strcspn(s, delim);
    do_str_cat(token, s, n);
    s += n;

    // return string tail or NULL if token empty
    assert(str_ok(*token));
    return token->len ? s : NULL;
}

// Read next character using escape character. Result in *out. Retuns tail pointer, and sets
// escaped=true if escape character parsed.
static const char *str_getc_esc(const char *s, char *out, bool *escaped, char esc) {
    if (*s != esc) {
        *escaped = false;
        *out = *s;
        return s + 1;
    } else {
        assert(*s && *s == esc);
        *escaped = true;
        *out = *(s + 1);
        return s + 2;
    }
}

const char *str_tok_esc(const char *s, str_t *token, char delim, char esc) {
    assert(str_ok(*token) && delim && esc);

    // empty tail: no-op
    if (!s)
        return NULL;

    // clear token
    str_resize(token, 0);

    const char *tail = s;
    char c;
    bool escaped, accumulate = false;

    while (*tail && (tail = str_getc_esc(tail, &c, &escaped, esc))) {
        if (!accumulate && (c != delim || escaped))
            accumulate = true;

        if (accumulate) {
            if (c != delim || escaped)
                str_push(token, c);
            else
                break;
        }
    }

    // return string tail or NULL if token empty
    assert(str_ok(*token));
    return token->len ? tail : NULL;
}

const char *str_prefix(const char *s, const char *prefix) {
    size_t len = strlen(prefix);
    return strncmp(s, prefix, len) ? NULL : s + len;
}

size_t str_getline(str_t *out, FILE *in) {
    assert(str_ok(*out) && in);
    str_resize(out, 0);
    int c;

    stdio_lock(in);

    while (true) {
#ifdef __MINGW32__
        c = _getc_nolock(in);
#else
        c = getc_unlocked(in);

        // Special case: reading a Windows encoded file (CR+LF) on a POSIX system
        if (c == '\r')
            continue;
#endif

        if (c != '\n' && c != EOF)
            str_push(out, (char)c);
        else
            break;
    }

    stdio_unlock(in);

    const size_t n = out->len + (c == '\n');

    assert(str_ok(*out));
    return n;
}

bool str_to_uintmax(const char *s, uintmax_t *result) {
    *result = 0;
    uintmax_t previous = 0;
    int c = 0;

    while ((c = *s++)) {
        if (isdigit(c)) {
            *result = 10 * *result + (uintmax_t)(c - '0');

            if (*result < previous)
                return false; // overflow
        } else
            return false;

        previous = *result;
    }

    return true;
}

bool str_to_uint8(const char *s, uint8_t *result) {
    uintmax_t tmp = 0;

    if (str_to_uintmax(s, &tmp) && tmp <= UINT8_MAX) {
        *result = (uint8_t)tmp;
        return true;
    } else
        return false;
}

bool str_to_uint16(const char *s, uint16_t *result) {
    uintmax_t tmp = 0;

    if (str_to_uintmax(s, &tmp) && tmp <= UINT16_MAX) {
        *result = (uint16_t)tmp;
        return true;
    } else
        return false;
}
