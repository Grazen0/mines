#include "csp.h"
#include "extra.h"
#include "memory.h"
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

[[nodiscard]] static constraint_t *cons_create(arena_t *arena, size_t n_vars,
                                               size_t rhs, rel_t rel)
{
    assert(arena);
    return cons_init(MAKE(arena, 1, constraint_t), n_vars, rhs, rel);
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

[[nodiscard]] static var_link_t *vlink_create(arena_t *arena,
                                              constraint_t *cons)
{
    assert(arena);
    return vlink_init(MAKE(arena, 1, var_link_t), cons);
}

static void vlink_prepend(var_link_t *list[static 1], var_link_t link[static 1])
{
    assert(list);
    assert(link);
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

[[nodiscard]] static csp_t *csp_init(csp_t *csp, arena_t *arena, size_t n_vars)
{
    if (!csp)
        return nullptr;

    auto var_links = MAKE(arena, n_vars, var_link_t *);
    if (!var_links)
        return nullptr;

    memset((void *)var_links, 0, n_vars * sizeof(*var_links));

    *csp = (csp_t){
        .constraints = nullptr,
        .n_vars = n_vars,
        .var_links = var_links,
    };

    return csp;
}

csp_t *csp_create(arena_t *arena, size_t n_vars)
{
    assert(arena);
    return csp_init(MAKE(arena, 1, csp_t), arena, n_vars);
}

bool csp_add_constraint(csp_t *csp, arena_t *arena, size_t n,
                        size_t const vars[static n], size_t rhs, rel_t rel)
{
    assert(csp);
    assert(arena);
    assert(vars);

    constraint_t *cons = cons_create(arena, n, rhs, rel);
    if (!cons)
        return false;

    cons_prepend(&csp->constraints, cons);

    for (size_t i = 0; i < n; ++i) {
        assert(vars[i] < csp->n_vars);
        var_link_t *link = vlink_create(arena, cons);
        if (!link)
            die("vlink_create");
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

[[nodiscard]] static solver_t *solver_init(solver_t *solver, arena_t *arena,
                                           csp_t const *csp, void *cb_ctx,
                                           csp_callback_t *cb)
{
    if (!solver) {
        return nullptr;
    }

    if (!arena || !csp) {
        errno = EINVAL;
        return nullptr;
    }

    auto vals = MAKE(arena, csp->n_vars, uint8_t);
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

bool csp_solve(csp_t const *csp, arena_t *arena, void *ctx, csp_callback_t *cb)
{
    assert(csp);
    assert(arena);
    assert(cb);

    solver_t solver = {};
    if (!solver_init(&solver, arena, csp, ctx, cb))
        return false;

    solver_backtrack(&solver, 0);
    return true;
}
