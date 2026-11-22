#pragma once
#include <stddef.h>

#define MAKE(a, n, T) \
    ((T *)(arena_aligned_alloc((a), alignof(T), sizeof(T) * (n))))
#define MAKE_2D(a, n, m, T) \
    ((T(*)[m])(arena_aligned_alloc((a), alignof(T), sizeof(T) * (n) * (m))))

typedef struct arena_t arena_t;

[[nodiscard]] arena_t *arena_create(size_t size);

void arena_destroy(arena_t *arena);

[[nodiscard]] void *arena_aligned_alloc(arena_t *arena, size_t align, size_t n);

[[nodiscard]] static inline void *arena_alloc(arena_t *arena, size_t n)
{
    return arena_aligned_alloc(arena, alignof(max_align_t), n);
}

void arena_clear(arena_t *arena);
