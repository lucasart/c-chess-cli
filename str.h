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
#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    char *restrict buf;  // C-string ('\0' terminated)
    size_t alloc;  // allocated size (including '\0' terminator)
    size_t len;  // number of characters for string content, excluding '\0' terminator
} str_t;

// checks if string 's' is valid
bool str_ok(str_t s);
bool str_eq(str_t s1, str_t s2);

// Make a string ref out of a C-string. This allows to use C-string as input to str_*() functions,
// without any copying, and without duplicating all functions.
str_t str_ref(const char *src);

// returns a string, copying its content from 'src'
str_t str_dup(str_t src);

void str_del(str_t *s);
#define scope(func) __attribute__ ((cleanup(func)))

#define str_del_n(...) do { \
    str_t *_s[] = {__VA_ARGS__}; \
    for (size_t _i = 0; _i < sizeof(_s) / sizeof(*_s); _i++) \
        str_del(_s[_i]); \
} while (0)

// copies 'src' into 'dest'
str_t *str_cpy(str_t *dest, str_t src);

// copy at most n characters of 'src' into 'dest'
str_t *str_ncpy(str_t *dest, str_t src, size_t n);

// append 'c' to 'dest'
str_t *str_push(str_t *dest, char c);

// appends at most n characters of 'src' to 'dest'
str_t *str_ncat(str_t *dest, str_t src, size_t n);

// String concatenation
str_t *str_cat(str_t *dest, str_t src);

// same as sprintf(), but appends, instead of replace, to valid string s1
void str_cat_fmt(str_t *dest, const char *fmt, ...);

// reads a token into valid string 'token', from s, using delim characters as a generalisation for
// white spaces. returns tail pointer on success, otherwise NULL (no more tokens to read).
const char *str_tok(const char *s, str_t *token, const char *delim);

// reads a line from file 'in', into valid string 'out', and return the number of characters read
// (including the '\n' if any). The '\n' is discarded from the output, but still counted.
size_t str_getline(str_t *out, FILE *in);
