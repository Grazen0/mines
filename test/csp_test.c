#include "csp.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

void setUp()
{
}

void tearDown()
{
}

typedef struct solution_t solution_t;
struct solution_t {
    uint8_t *vals;
    solution_t *next;
};

static void sol_destroy(solution_t *sol)
{
    if (!sol)
        return;

    sol_destroy(sol->next);
    free(sol->vals);
    free(sol);
}

typedef struct {
    solution_t *sols;
} result_t;

static void solver_cb(void *ctx, size_t n, uint8_t const vals[static n])
{
    result_t *res = ctx;

    uint8_t *vals_cpy = calloc(n, sizeof(*vals));
    assert(vals_cpy);
    memcpy(vals_cpy, vals, n * sizeof(*vals));

    solution_t *sol = calloc(1, sizeof(*sol));
    assert(sol);

    *sol = (solution_t){
        .vals = vals_cpy,
        .next = res->sols,
    };
    res->sols = sol;
}

void test_csp_misc()
{
    static constexpr size_t N = 4;
    static constexpr size_t C1[] = {0, 1, 2};
    static constexpr size_t C2[] = {2};

    csp_t *csp = csp_create(N);

    // x0 + x1 + x2 <= 4
    //           x2 = 1
    TEST_ASSERT_TRUE(csp_add_constraint(csp, countof(C1), C1, 2, REL_LEQ));
    TEST_ASSERT_TRUE(csp_add_constraint(csp, countof(C2), C2, 1, REL_EQ));

    result_t res = (result_t){};
    TEST_ASSERT_TRUE(csp_solve(csp, &res, solver_cb));
    for (auto sol = res.sols; sol; sol = sol->next) {
        uint8_t *x = sol->vals;
        TEST_ASSERT_LESS_OR_EQUAL(4, x[0] + x[1] + x[2]);
        TEST_ASSERT_EQUAL(1, x[2]);
    }

    sol_destroy(res.sols);
    csp_destroy(csp);
}
