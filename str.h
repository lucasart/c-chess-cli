/* Dynamic strings. Example:
    str_t s1 = str_new();  // s1 = ""
    str_t s2 = str_dup("world");  // s2 = "world"
    str_cpy(&s1, "hello");  // s1 = "hello"
    str_cat(&s1, " ", s2.buf);  // s1 += " " + s2 (appends any n >= 1 strings to s1)
    str_putc(&s1, '!');  // s1 += '!' (also accepts any n >= 1 RHS elements)
    puts(s1.buf);  // displays "hello world!"
    free(&s1, &s2);  // frees both strings (any n >= 1 arguments)
*/
#pragma once
#include <stdio.h>

typedef struct {
    char *buf;  // always '\0' terminated
    size_t alloc;  // allocated size (includes '\0' terminator)
    size_t len;  // does not count '\0' terminator
} str_t;

str_t str_new();
str_t str_dup(const char *src);
void str_resize(str_t *s, size_t len);

void str_cpy(str_t *dest, const char *src);
#define str_putc(...) str_putc_aux(__VA_ARGS__, NULL)
void str_ncat(str_t *dest, const char *src, size_t n);
#define str_cat(...) str_cat_aux(__VA_ARGS__, NULL)
void str_catf(str_t *s1, const char *fmt, ...);
#define str_free(...) str_free_aux(__VA_ARGS__, NULL)

const char *str_tok(const char *s, str_t *token, const char *delim);
size_t str_getdelim(str_t *out, int delim, FILE *in);

// Don't use the _aux() functions directly, use the variadic macros instead
void str_putc_aux(str_t *dest, int c1, ...);
void str_cat_aux(str_t *dest, const char *s1, ...);
void str_free_aux(str_t *s1, ...);
