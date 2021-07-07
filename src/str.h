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
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *restrict buf; // C-string ('\0' terminated)
    size_t alloc;       // allocated size (including '\0' terminator)
    size_t len;         // number of characters for string content, excluding '\0' terminator
} str_t;

// checks if string 's' is valid
bool str_ok(str_t s);
bool str_eq(str_t s1, str_t s2);

// Make a string ref out of a C-string. This allows to use C-string as input to str_*() functions,
// without any copying, and without duplicating all functions (just adding macros for convience).
str_t str_ref(const char *src);

// constructors
str_t str_init(void);                                        // empty string ""
str_t str_init_from(str_t src);                              // copy from string
#define str_init_from_c(c_str) str_init_from(str_ref(c_str)) // copy from C-string

// set s = "", but faster than using str_cpy_c(s, "")
void str_clear(str_t *s);

void str_destroy(str_t *s);
#define scope(func) __attribute__((cleanup(func)))

#define str_destroy_n(...)                                                                         \
    do {                                                                                           \
        str_t *_s[] = {__VA_ARGS__};                                                               \
        for (size_t _i = 0; _i < sizeof(_s) / sizeof(*_s); _i++)                                   \
            str_destroy(_s[_i]);                                                                   \
    } while (0)

// copies 'src' into 'dest'
str_t *str_cpy(str_t *dest, str_t src);
#define str_cpy_c(dest, c_str) str_cpy(dest, str_ref(c_str))

// copy at most n characters of 'src' into 'dest'
str_t *str_ncpy(str_t *dest, str_t src, size_t n);

// append 'c' to 'dest'
str_t *str_push(str_t *dest, char c);

// appends at most n characters of 'src' to 'dest'
str_t *str_ncat(str_t *dest, str_t src, size_t n);

// String concatenation
str_t *str_cat(str_t *dest, str_t src);
#define str_cat_c(dest, c_str) str_cat(dest, str_ref(c_str))

// Lightweight functions to concatenate a single integer (decimal signed/unsined)
str_t *str_cat_int(str_t *dest, intmax_t i);
str_t *str_cat_uint(str_t *dest, uintmax_t u);

// same as sprintf(), but provides both replace (cpy) and append (cat) versions
str_t *str_cpy_fmt(str_t *dest, const char *fmt, ...);
str_t *str_cat_fmt(str_t *dest, const char *fmt, ...);

// reads a token into valid string 'token', from s, using delim characters as a generalisation for
// white spaces. returns tail pointer on success, otherwise NULL (no more tokens to read).
const char *str_tok(const char *s, str_t *token, const char *delim);

// Similar to str_tok(), but single delimiter, and using escape character. For example:
// s = "alice\ bob charlie", delim=' ', esc='\' => token="alice bob", returns tail="charlie"
const char *str_tok_esc(const char *s, str_t *token, char delim, char esc);

// If s starts with prefix, return the tail (from s = prefix + tail), otherwise return NULL.
const char *str_prefix(const char *s, const char *prefix);

// reads a line from file 'in', into valid string 'out', and return the number of characters read
// (including the '\n' if any). The '\n' is discarded from the output, but still counted.
size_t str_getline(str_t *out, FILE *in);

bool str_to_uintmax(const char *s, uintmax_t *result);
bool str_to_uint8(const char *s, uint8_t *result);
bool str_to_uint16(const char *s, uint16_t *result);
