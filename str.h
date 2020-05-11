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
#include <stdio.h>

typedef struct {
    char *buf;  // C-string ('\0' terminated)
    size_t alloc;  // allocated size (including '\0' terminator)
    size_t len;  // number of characters for string content, excluding '\0' terminator
} str_t;

// checks if string 's' is valid
bool str_ok(const str_t *s);

// returns a valid string with value ""
str_t str_new();

// returns a valid string, copying its content from C-string 'src'
str_t str_dup(const char *src);

// str_delete(&s1, ..., &sn): frees a list of n >= 1 valid strings, and marks them as invalid
#define str_delete(...) str_free_aux(__VA_ARGS__, NULL)
void str_free_aux(str_t *s1, ...);

// copies C-string 'src' into valid string 'dest'
void str_cpy(str_t *dest, const char *restrict src);

// copy at most n characters of C-string 'src' into valid string 'dest'
void str_ncpy(str_t *dest, const char *restrict src, size_t n);

// str_putc(&dest, c1, ..., cn): appends n >= 1 characters c1...cn, to a valid string 'dest'
#define str_putc(...) str_putc_aux(__VA_ARGS__, NULL)
void str_putc_aux(str_t *dest, int c1, ...);

// appends at most n characters of C-string 'src' into valid string 'dest'
void str_ncat(str_t *dest, const char *src, size_t n);

// str_cat(&dest, &s1, ..., &sn) appends n >= 1 C-strings to a valid string dest
#define str_cat(...) str_cat_aux(__VA_ARGS__, NULL)
void str_cat_aux(str_t *dest, const char *s1, ...);

// same as sprintf(), but appends, instead of replace, to valid string s1
void str_catf(str_t *dest, const char *fmt, ...);

// reads a token into valid string 'token', from s, using delim characters as a generalisation for
// white spaces. returns tail pointer on success, otherwise NULL (no more tokens to read).
const char *str_tok(const char *s, str_t *token, const char *delim);

// reads a line from file 'in', into valid string 'out', and return the number of characters read
// (including the '\n' if any). When trim = true, the '\n' is discarded (but counted).
size_t str_getline(str_t *out, FILE *in, bool trim);
