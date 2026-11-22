#include "memory.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct arena_t {
    size_t size;
    size_t off;
    alignas(max_align_t) uint8_t base[];
};
static_assert(offsetof(arena_t, base) % alignof(max_align_t) == 0,
              "arena.base is not properly aligned");

[[nodiscard]] static arena_t *arena_init(arena_t *arena, size_t size)
{
    if (!arena)
        return nullptr;

    arena->size = size;
    arena->off = 0;
    return arena;
}

arena_t *arena_create(size_t size)
{
    return arena_init(malloc(sizeof(arena_t) + size), size);
}

void arena_destroy(arena_t *arena)
{
    if (!arena)
        return;

    free(arena);
}

static inline size_t align_up_to(size_t n, size_t align)
{
    return (n + align - 1) & -align;
}

void *arena_aligned_alloc(arena_t *arena, size_t align, size_t n)
{
    assert(arena);

    uintptr_t off_aligned = align_up_to(arena->off, align);
    assert(off_aligned % align == 0);

    if (off_aligned > SIZE_MAX - n) {
        errno = ENOMEM;
        return nullptr;
    }

    uintptr_t new_off = off_aligned + n;

    if (new_off > arena->size) {
        errno = ENOMEM;
        return nullptr;
    }

    arena->off = new_off;
    return arena->base + off_aligned;
}

void arena_clear(arena_t *arena)
{
    assert(arena);
    arena->off = 0;
}
