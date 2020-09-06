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

void *vec_do_new(size_t capacity, size_t esize);
#define vec_new(capacity, etype) vec_do_new(capacity, sizeof(etype))

void vec_del(void *v);

size_t vec_size(const void *v);
size_t vec_capacity(const void *v);

void vec_clear(void *v);

void *vec_do_realloc(void *v, size_t esize, size_t n);

#define vec_push(v, e) ({ \
    const vec_t *p = vec_cptr(v); \
    if (p->capacity == p->size) \
        v = vec_do_realloc(v, sizeof(*v), p->capacity ? 2 * p->capacity : 1); \
    (v)[vec_ptr(v)->size++] = (e); \
})

#define vec_pop(v) \
    ((v)[--vec_ptr(v)->size])
