/*
 * This is free and unencumbered software released into the public domain.
 * Based on https://github.com/skeeto/growable-buf
 */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    size_t capacity;
    size_t size;
    char buf[];
} vec_t;

vec_t *vec_ptr(void *v);
const vec_t *vec_cptr(const void *v);

void *vec_new(void);
void vec_del(void *v);

size_t vec_size(const void *v);
size_t vec_capacity(const void *v);

void *vec_do_grow(void *v, size_t esize, size_t n);

#define vec_push(v, e) \
    do { \
        if (vec_capacity((v)) == vec_size((v))) \
            (v) = vec_do_grow(v, sizeof(*(v)), !vec_capacity((v)) ? 1 : vec_capacity((v))); \
        (v)[vec_ptr((v))->size++] = (e); \
    } while (0)

#define vec_pop(v) \
    ((v)[--vec_ptr(v)->size])

#define vec_grow(v, n) \
    ((v) = vec_do_grow((v), sizeof(*(v)), n))

#define vec_clear(v) \
    (vec_ptr(v)->size = 0)
