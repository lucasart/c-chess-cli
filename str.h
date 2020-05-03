#pragma once
#include <stddef.h>

typedef struct {
    char *buf;  // always '\0' terminated
    size_t alloc;  // allocated size (includes '\0' terminator)
    size_t len;  // does not count '\0' terminator
} str_t;

str_t str_new(const char *src);
void str_resize(str_t *s, size_t len);
void str_free(str_t *s);

str_t *str_cpy(str_t *dest, const char *src);
str_t *str_cat(str_t *dest, const char *src);
str_t *str_putc(str_t *dest, char c);

size_t str_getdelim(str_t *str, int delim, FILE *f);
