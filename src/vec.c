/*
 * This is free and unencumbered software released into the public domain.
 * Based on https://github.com/skeeto/growable-buf
 */
#include "vec.h"

vec_t *vec_ptr(void *v)
{
    return (vec_t *)((char *)(v) - offsetof(vec_t, buf));
}

const vec_t *vec_cptr(const void *v)
{
    return (const vec_t *)((const char *)(v) - offsetof(vec_t, buf));
}

void *vec_do_new(size_t capacity, size_t esize)
{
    vec_t *p = malloc(sizeof(vec_t) + capacity * esize);
    p->capacity = capacity;
    p->size = 0;
    return (void *)p->buf;
}

void vec_del(void *v)
{
    free(vec_ptr(v));
}

size_t vec_size(const void *v)
{
    return vec_cptr(v)->size;
}

size_t vec_capacity(const void *v)
{
    return vec_cptr(v)->capacity;
}

void *vec_do_grow(void *v, size_t esize, size_t n)
{
    assert(v);
    vec_t *p;

    p = vec_ptr(v);
    p = realloc(p, sizeof(vec_t) + esize * (p->capacity + n));
    p->capacity += n;

    if (p->size > p->capacity)
        p->size = p->capacity;

    return p->buf;
}
