#include "csp.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Represents a constraint of the form x_1 + x_2 + ... + x_n = rhs
typedef struct constraint_t constraint_t;
struct constraint_t {
    constraint_t *next;
    size_t n_vars;
    size_t rhs;
    rel_t rel;
};

[[nodiscard]] static constraint_t *cons_init(constraint_t *cons, size_t n_vars,
                                             size_t rhs, rel_t rel)
{
    if (!cons)
        return nullptr;

    *cons = (constraint_t){
        .n_vars = n_vars,
        .rhs = rhs,
        .rel = rel,
    };

    return cons;
}

[[nodiscard]] static constraint_t *cons_create(size_t n_vars, size_t rhs,
                                               rel_t rel)
{
    return cons_init(malloc(sizeof(constraint_t)), n_vars, rhs, rel);
}

static void cons_destroy(constraint_t *cons);

static void cons_deinit(constraint_t *cons)
{
    if (!cons)
        return;

    cons_destroy(cons->next);
}

static void cons_destroy(constraint_t *cons)
{
    cons_deinit(cons);
    free(cons);
}

static void cons_prepend(constraint_t **list, constraint_t *cons)
{
    cons->next = *list;
    *list = cons;
}

[[nodiscard]] static bool cons_allows_sub(constraint_t const cons[static 1],
                                          uint8_t val)
{
    assert(cons);
    assert(val <= 1);

    // - there must be at least one variable to substitute
    // - rhs must allow subtracting val
    if (cons->n_vars == 0 || cons->rhs < val)
        return false;

    // if constraint is ==, the number of vars left after substituting should be
    // able to reach the rhs, at least, if all remaining vars are set to 1
    if (cons->rel == REL_EQ && cons->n_vars - 1 < cons->rhs - val)
        return false;

    return true;
}

static void cons_apply_sub(constraint_t cons[static 1], uint8_t val)
{
    assert(cons);
    assert(val <= 1);
    assert(cons_allows_sub(cons, val));

    cons->rhs -= val;
    --cons->n_vars;
}

static void cons_undo_sub(constraint_t cons[static 1], uint8_t val)
{
    assert(cons);
    assert(val <= 1);

    cons->rhs += val;
    ++cons->n_vars;
}

// Represents a list of constraints where a variable is present.
typedef struct var_link_t var_link_t;
struct var_link_t {
    constraint_t *cons;
    var_link_t *next;
};

[[nodiscard]] static var_link_t *vlink_init(var_link_t *link,
                                            constraint_t *cons)
{
    if (!link)
        return nullptr;

    if (!cons) {
        errno = EINVAL;
        return nullptr;
    }

    *link = (var_link_t){
        .cons = cons,
        .next = nullptr,
    };
    return link;
}

[[nodiscard]] static var_link_t *vlink_create(constraint_t *cons)
{
    return vlink_init(malloc(sizeof(var_link_t)), cons);
}

static void vlink_destroy(var_link_t *link);

static void vlink_deinit(var_link_t *link)
{
    if (!link)
        return;

    vlink_destroy(link->next);
}

static void vlink_destroy(var_link_t *link)
{
    vlink_deinit(link);
    free(link);
}

static void vlink_prepend(var_link_t **list, var_link_t *link)
{
    link->next = *list;
    *list = link;
}

struct csp_t {
    // Number of variables in the CSP.
    size_t n_vars;

    // Linked list of constraints.
    constraint_t *constraints;

    // Dynamic array that maps each variable to
    // its linked list of associated constraints.
    var_link_t **var_links;
};

[[nodiscard]] static csp_t *csp_init(csp_t *csp, size_t n_vars)
{
    if (!csp)
        return nullptr;

    var_link_t **var_links = (var_link_t **)calloc(n_vars, sizeof(*var_links));
    if (!var_links)
        return nullptr;

    *csp = (csp_t){
        .constraints = nullptr,
        .n_vars = n_vars,
        .var_links = var_links,
    };

    return csp;
}

csp_t *csp_create(size_t n_vars)
{
    return csp_init(malloc(sizeof(csp_t)), n_vars);
}

void csp_deinit(csp_t *csp)
{
    if (!csp)
        return;

    for (size_t i = 0; i < csp->n_vars; ++i)
        vlink_destroy(csp->var_links[i]);
    free((void *)csp->var_links);
    cons_destroy(csp->constraints);
    *csp = (csp_t){};
}

void csp_destroy(csp_t *csp)
{
    csp_deinit(csp);
    free(csp);
}

bool csp_add_constraint(csp_t *csp, size_t n, size_t const vars[static n],
                        size_t rhs, rel_t rel)
{
    constraint_t *cons = cons_create(n, rhs, rel);
    if (!cons)
        return false;

    cons_prepend(&csp->constraints, cons);

    for (size_t i = 0; i < n; ++i) {
        assert(vars[i] < csp->n_vars);
        var_link_t *link = vlink_create(cons);
        vlink_prepend(&csp->var_links[vars[i]], link);
    }

    return true;
}

static constexpr uint8_t VAL_UNSET = (uint8_t)-1;

typedef struct {
    csp_t const *csp;
    uint8_t *vals;
    void *cb_ctx;
    csp_callback_t *cb;
} solver_t;

[[nodiscard]] static solver_t *solver_init(solver_t *solver, csp_t const *csp,
                                           void *cb_ctx, csp_callback_t *cb)
{
    if (!solver) {
        return nullptr;
    }

    if (!csp) {
        errno = EINVAL;
        return nullptr;
    }

    uint8_t *vals = calloc(csp->n_vars, sizeof(*vals));
    if (!vals)
        return nullptr;

    for (size_t i = 0; i < csp->n_vars; ++i)
        vals[i] = VAL_UNSET;

    *solver = (solver_t){
        .csp = csp,
        .vals = vals,
        .cb_ctx = cb_ctx,
        .cb = cb,
    };

    return solver;
}

static void solver_deinit(solver_t *solver)
{
    if (!solver)
        return;

    free(solver->vals);
}

[[nodiscard]] static bool solver_allows_value(solver_t const solver[static 1],
                                              size_t x, uint8_t val)
{
    assert(solver);
    assert(x < solver->csp->n_vars);
    assert(solver->vals[x] == VAL_UNSET);

    var_link_t *head = solver->csp->var_links[x];

    for (auto link = head; link; link = link->next) {
        if (!cons_allows_sub(link->cons, val))
            return false;
    }

    return true;
}

static void solver_set_var(solver_t solver[static 1], size_t x, uint8_t val)
{
    assert(solver);
    assert(x < solver->csp->n_vars);
    assert(solver->vals[x] == VAL_UNSET);
    assert(solver_allows_value(solver, x, val));

    var_link_t *head = solver->csp->var_links[x];

    for (auto link = head; link; link = link->next)
        cons_apply_sub(link->cons, val);

    solver->vals[x] = val;
}

static void solver_unset_var(solver_t solver[static 1], size_t x)
{
    assert(solver);
    assert(x < solver->csp->n_vars);
    assert(solver->vals[x] != VAL_UNSET);

    var_link_t *head = solver->csp->var_links[x];

    for (auto link = head; link; link = link->next)
        cons_undo_sub(link->cons, solver->vals[x]);

    solver->vals[x] = VAL_UNSET;
}

static void solver_backtrack(solver_t solver[static 1], size_t x)
{
    assert(solver);
    assert(x <= solver->csp->n_vars);

    if (x == solver->csp->n_vars) {
        solver->cb(solver->cb_ctx, solver->csp->n_vars, solver->vals);
        return;
    }

    for (uint8_t v = 0; v <= 1; ++v) {
        if (solver_allows_value(solver, x, v)) {
            solver_set_var(solver, x, v);
            solver_backtrack(solver, x + 1);
            solver_unset_var(solver, x);
        }
    }
}

bool csp_solve(csp_t const *csp, void *ctx, csp_callback_t *cb)
{
    assert(csp);
    assert(cb);

    solver_t solver = {};
    if (!solver_init(&solver, csp, ctx, cb))
        return false;

    solver_backtrack(&solver, 0);
    solver_deinit(&solver);
    return true;
}
