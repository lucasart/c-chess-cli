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

#define vec_ptr(v) \
    ((vec_t *)((char *)(v) - offsetof(vec_t, buf)))

#define vec_new() ({ \
    vec_t *p = malloc(sizeof(vec_t)); \
    p->size = p->capacity = 0; \
    (void *)p->buf; \
})

#define vec_free(v) \
    free(vec_ptr((v)))

#define vec_size(v) \
    vec_ptr((v))->size

#define vec_capacity(v) \
    vec_ptr((v))->capacity

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
    (vec_ptr((v))->size = 0)

static void *vec_do_grow(void *v, size_t esize, size_t n)
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
