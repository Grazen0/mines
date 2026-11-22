#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum : uint8_t {
    REL_EQ, // ==
    REL_LEQ, // <=
} rel_t;

typedef struct csp_t csp_t;

[[nodiscard]] csp_t *csp_create(size_t n_vars);

void csp_destroy(csp_t *csp);

/**
 * @brief Adds a constraint to the CSP.
 *
 * The added constraint is of the following form:
 *
 *   vars[0] + vars[1] + ... + vars[n-1] = rhs
 *
 * @param csp the CSP to add the constraint to
 * @param n length of vars
 * @param vars variables in the left-hand side
 *
 * @return true on success, false on error.
 */
[[nodiscard]] bool csp_add_constraint(csp_t *csp, size_t n,
                                      size_t const vars[static n], size_t rhs,
                                      rel_t rel);

typedef void csp_callback_t(void *ctx, size_t n, uint8_t const vals[static n]);

/**
 * @brief Solves the CSP, calling `cb` for every solution.
 *
 * @return true on success, false on error.
 */
[[nodiscard]] bool csp_solve(csp_t const *csp, void *ctx, csp_callback_t *cb);
