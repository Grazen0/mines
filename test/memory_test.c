#include "memory.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

static arena_t *arena;

void setUp()
{
    arena = nullptr;
}

void tearDown()
{
    arena_destroy(arena);
}

void test_arena_allocation_fits_exact_capacity()
{
    arena = arena_create(32);
    TEST_ASSERT_NOT_NULL(arena);

    uint8_t *ptr = arena_aligned_alloc(arena, 1, 32);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 0xA5, 32);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xA5, ptr, 32);

    errno = 0;
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, 1));
    TEST_ASSERT_EQUAL_INT(ENOMEM, errno);
}

void test_arena_allocation_larger_than_capacity_fails()
{
    arena = arena_create(16);
    TEST_ASSERT_NOT_NULL(arena);

    errno = 0;
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, 17));
    TEST_ASSERT_EQUAL_INT(ENOMEM, errno);
}

void test_arena_empty_cannot_allocate_one_byte()
{
    arena = arena_create(0);
    TEST_ASSERT_NOT_NULL(arena);

    errno = 0;
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, 1));
    TEST_ASSERT_EQUAL_INT(ENOMEM, errno);
}

void test_arena_multiple_allocations_exhaust_capacity()
{
    arena = arena_create(16);
    TEST_ASSERT_NOT_NULL(arena);

    TEST_ASSERT_NOT_NULL(arena_aligned_alloc(arena, 1, 5));
    TEST_ASSERT_NOT_NULL(arena_aligned_alloc(arena, 1, 7));
    TEST_ASSERT_NOT_NULL(arena_aligned_alloc(arena, 1, 4));
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, 1));
}

void test_arena_failed_allocation_preserves_remaining_capacity()
{
    arena = arena_create(16);
    TEST_ASSERT_NOT_NULL(arena);

    uint8_t *first = arena_aligned_alloc(arena, 1, 5);
    TEST_ASSERT_NOT_NULL(first);
    memset(first, 0xA5, 5);
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, 12));

    uint8_t *second = arena_aligned_alloc(arena, 1, 11);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_PTR(first + 5, second);
    memset(second, 0x5A, 11);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xA5, first, 5);
}

void test_arena_aligned_allocation_includes_padding()
{
    static constexpr size_t ALIGN = alignof(max_align_t);
    arena = arena_create(2 * ALIGN);
    TEST_ASSERT_NOT_NULL(arena);

    uint8_t *first = arena_aligned_alloc(arena, 1, 1);
    TEST_ASSERT_NOT_NULL(first);
    uint8_t *second = arena_aligned_alloc(arena, ALIGN, ALIGN);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_TRUE((uintptr_t)second % ALIGN == 0);
    TEST_ASSERT_EQUAL_PTR(first + ALIGN, second);
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, 1));
}

void test_arena_alignment_padding_can_make_allocation_fail()
{
    static constexpr size_t ALIGN = alignof(max_align_t);
    arena = arena_create((2 * ALIGN) - 1);
    TEST_ASSERT_NOT_NULL(arena);

    uint8_t *first = arena_aligned_alloc(arena, 1, 1);
    TEST_ASSERT_NOT_NULL(first);
    errno = 0;
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, ALIGN, ALIGN));
    TEST_ASSERT_EQUAL_INT(ENOMEM, errno);

    // The failed request must not consume its alignment padding.
    uint8_t *second = arena_aligned_alloc(arena, 1, (2 * ALIGN) - 2);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_PTR(first + 1, second);
}

void test_arena_alloc_uses_requested_size_and_default_alignment()
{
    static constexpr size_t ALIGN = alignof(max_align_t);
    arena = arena_create((2 * ALIGN) + 3);
    TEST_ASSERT_NOT_NULL(arena);

    uint8_t *first = arena_alloc(arena, 3);
    TEST_ASSERT_NOT_NULL(first);
    uint8_t *second = arena_alloc(arena, 3);
    TEST_ASSERT_NOT_NULL(second);
    uint8_t *third = arena_alloc(arena, 3);
    TEST_ASSERT_NOT_NULL(third);

    TEST_ASSERT_TRUE((uintptr_t)first % ALIGN == 0);
    TEST_ASSERT_TRUE((uintptr_t)second % ALIGN == 0);
    TEST_ASSERT_TRUE((uintptr_t)third % ALIGN == 0);
    TEST_ASSERT_EQUAL_PTR(first + ALIGN, second);
    TEST_ASSERT_EQUAL_PTR(first + (2 * ALIGN), third);
    memset(first, 0x11, 3);
    memset(second, 0x22, 3);
    memset(third, 0x33, 3);
    TEST_ASSERT_EACH_EQUAL_UINT8(0x11, first, 3);
    TEST_ASSERT_EACH_EQUAL_UINT8(0x22, second, 3);
    TEST_ASSERT_EACH_EQUAL_UINT8(0x33, third, 3);
    TEST_ASSERT_NULL(arena_alloc(arena, 1));
}

void test_arena_alloc_rejects_request_larger_than_capacity()
{
    arena = arena_create(32);
    TEST_ASSERT_NOT_NULL(arena);

    errno = 0;
    TEST_ASSERT_NULL(arena_alloc(arena, 64));
    TEST_ASSERT_EQUAL_INT(ENOMEM, errno);
}

void test_arena_allocated_regions_do_not_overlap()
{
    static constexpr size_t sizes[] = {3, 5, 1, 7, 9};
    static constexpr size_t aligns[] = {1, 2, alignof(max_align_t), 4, 1};
    uint8_t *regions[countof(sizes)] = {};
    arena = arena_create(128);
    TEST_ASSERT_NOT_NULL(arena);

    for (size_t i = 0; i < countof(sizes); ++i) {
        regions[i] = arena_aligned_alloc(arena, aligns[i], sizes[i]);
        TEST_ASSERT_NOT_NULL(regions[i]);
        TEST_ASSERT_TRUE((uintptr_t)regions[i] % aligns[i] == 0);

        for (size_t j = 0; j < i; ++j) {
            uintptr_t a = (uintptr_t)regions[i];
            uintptr_t b = (uintptr_t)regions[j];
            TEST_ASSERT_TRUE(a + sizes[i] <= b || b + sizes[j] <= a);
        }

        memset(regions[i], (int)(i + 1), sizes[i]);
        // Each new write must leave all previous allocations intact.
        for (size_t j = 0; j <= i; ++j)
            TEST_ASSERT_EACH_EQUAL_UINT8(j + 1, regions[j], sizes[j]);
    }
}

void test_arena_overflowing_allocation_fails_without_consuming_space()
{
    arena = arena_create(16);
    TEST_ASSERT_NOT_NULL(arena);

    uint8_t *first = arena_aligned_alloc(arena, 1, 1);
    TEST_ASSERT_NOT_NULL(first);

    errno = 0;
    TEST_ASSERT_NULL(arena_aligned_alloc(arena, 1, SIZE_MAX));
    TEST_ASSERT_EQUAL_INT(ENOMEM, errno);

    uint8_t *second = arena_aligned_alloc(arena, 1, 15);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_PTR(first + 1, second);
}
