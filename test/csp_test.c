#include "csp.h"
#include "memory.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

static arena_t *arena;

void setUp()
{
    arena = arena_create(4096);
    TEST_ASSERT_NOT_NULL(arena);
}

void tearDown()
{
    arena_destroy(arena);
    arena = nullptr;
}

typedef struct solution_t solution_t;
struct solution_t {
    uint8_t *vals;
    solution_t *next;
};

typedef struct {
    solution_t *sols;
} result_t;

static void solver_cb(void *ctx, size_t n, uint8_t const vals[static n])
{
    result_t *res = ctx;

    auto vals_cpy = MAKE(arena, n, uint8_t);
    TEST_ASSERT_NOT_NULL(vals_cpy);
    memcpy(vals_cpy, vals, n * sizeof(*vals));

    auto sol = MAKE(arena, 1, solution_t);
    TEST_ASSERT_NOT_NULL(sol);

    *sol = (solution_t){
        .vals = vals_cpy,
        .next = res->sols,
    };
    res->sols = sol;
}

// Encode assignments as bit masks, with x0 in the least significant bit.
// Check the complete solution set, including missing or duplicate solutions.
static void assert_solutions(csp_t *csp, size_t n, size_t expected_count,
                             uint32_t const *expected)
{
    result_t res = {};
    TEST_ASSERT_TRUE(csp_solve(csp, arena, &res, solver_cb));

    size_t count = 0;
    for (auto sol = res.sols; sol; sol = sol->next)
        ++count;
    TEST_ASSERT_EQUAL_UINT(expected_count, count);

    for (size_t i = 0; i < expected_count; ++i) {
        size_t matches = 0;
        for (auto sol = res.sols; sol; sol = sol->next) {
            uint32_t assignment = 0;
            for (size_t j = 0; j < n; ++j) {
                TEST_ASSERT_LESS_OR_EQUAL_UINT8(1, sol->vals[j]);
                assignment |= (uint32_t)sol->vals[j] << j;
            }
            if (assignment == expected[i])
                ++matches;
        }
        TEST_ASSERT_EQUAL_UINT(1, matches);
    }
}

void test_csp_contradictory_single_variable()
{
    static constexpr size_t vars[] = {0};
    csp_t *csp = csp_create(arena, 1);
    TEST_ASSERT_NOT_NULL(csp);

    // x0 = 0 and x0 = 1.
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 0, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 1, REL_EQ));

    assert_solutions(csp, 1, 0, nullptr);
}

void test_csp_inconsistent_pairwise_sums()
{
    static constexpr size_t C1[] = {0, 1};
    static constexpr size_t C2[] = {1, 2};
    static constexpr size_t C3[] = {0, 2};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    // Every pair sums to 1, which has no binary solution.
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C1), C1, 1, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C2), C2, 1, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C3), C3, 1, REL_EQ));

    assert_solutions(csp, 3, 0, nullptr);
}

void test_csp_equality_exceeds_upper_bound()
{
    static constexpr size_t vars[] = {0, 1, 2};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    // x0 + x1 + x2 = 2 and x0 + x1 + x2 <= 1.
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 2, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 1, REL_LEQ));

    assert_solutions(csp, 3, 0, nullptr);
}

void test_csp_unique_solution_from_overlapping_constraints()
{
    static constexpr size_t C1[] = {0, 1};
    static constexpr size_t C2[] = {1, 2};
    static constexpr size_t C3[] = {0, 2};
    static constexpr uint32_t expected[] = {0b010};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    // x0 + x1 = 1, x1 + x2 = 1, x0 + x2 = 0.
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C1), C1, 1, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C2), C2, 1, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C3), C3, 0, REL_EQ));

    assert_solutions(csp, 3, countof(expected), expected);
}

void test_csp_zero_upper_bound_forces_all_zero()
{
    static constexpr size_t vars[] = {0, 1, 2};
    static constexpr uint32_t expected[] = {0b000};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 0, REL_LEQ));

    assert_solutions(csp, 3, countof(expected), expected);
}

void test_csp_maximum_sum_forces_all_one()
{
    static constexpr size_t vars[] = {0, 1, 2};
    static constexpr uint32_t expected[] = {0b111};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 3, REL_EQ));

    assert_solutions(csp, 3, countof(expected), expected);
}

void test_csp_exactly_one_of_three()
{
    static constexpr size_t vars[] = {0, 1, 2};
    static constexpr uint32_t expected[] = {0b001, 0b010, 0b100};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(vars), vars, 1, REL_EQ));

    assert_solutions(csp, 3, countof(expected), expected);
}

void test_csp_overlapping_equalities_have_two_solutions()
{
    static constexpr size_t C1[] = {0, 1};
    static constexpr size_t C2[] = {1, 2};
    static constexpr uint32_t expected[] = {0b010, 0b101};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    // x0 + x1 = 1 and x1 + x2 = 1.
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C1), C1, 1, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C2), C2, 1, REL_EQ));

    assert_solutions(csp, 3, countof(expected), expected);
}

void test_csp_mixed_constraints_have_three_solutions()
{
    static constexpr size_t C1[] = {0, 1};
    static constexpr size_t C2[] = {1, 2};
    static constexpr uint32_t expected[] = {0b001, 0b010, 0b101};
    csp_t *csp = csp_create(arena, 3);
    TEST_ASSERT_NOT_NULL(csp);

    // x0 + x1 = 1 and x1 + x2 <= 1.
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C1), C1, 1, REL_EQ));
    TEST_ASSERT_TRUE(
        csp_add_constraint(csp, arena, countof(C2), C2, 1, REL_LEQ));

    assert_solutions(csp, 3, countof(expected), expected);
}

void test_csp_unconstrained_variables()
{
    static constexpr uint32_t expected[] = {0b00, 0b01, 0b10, 0b11};
    csp_t *csp = csp_create(arena, 2);
    TEST_ASSERT_NOT_NULL(csp);

    assert_solutions(csp, 2, countof(expected), expected);
}
