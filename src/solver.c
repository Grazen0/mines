#include "solver.h"
#include "csp.h"
#include "extra.h"
#include "game.h"
#include "memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// solvers based on https://minesweeperblast.com/minesweeper-solver-algorithms/

[[nodiscard]] static bool are_neighbors(uint16_t i1, uint16_t j1, uint16_t i2,
                                        uint16_t j2)
{
    assert(i1 != i2 || j1 != j2);
    return abs((int)i1 - i2) <= 1 && abs((int)j1 - j2) <= 1;
}

[[nodiscard]] static bool is_boundary_clue(game_t const game[static 1],
                                           uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto revealed = GAME_UNFLAT_CONST(game, revealed);

    if (!revealed[i][j])
        return false;

    ITER_NEIGHBORS(game, i, j, ni, nj)
    {
        if (game_is_unknown(game, ni, nj))
            return true;
    }

    return false;
}

[[nodiscard]] static bool flag_all_neighbors(game_t game[static 1], uint16_t i,
                                             uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));
    bool retval = false;

    ITER_NEIGHBORS(game, i, j, ni, nj)
    {
        if (game_is_unknown(game, ni, nj)) {
            assert(game_toggle_flag(game, ni, nj));
            retval = true;
        }
    }

    return retval;
}

/**
 * @brief Checks whether S(i_a, j_a) is a subset of S(i_b, j_b).
 */
[[nodiscard]] static bool is_subset_of(game_t const game[static 1],
                                       uint16_t i_a, uint16_t j_a, uint16_t i_b,
                                       uint16_t j_b)
{
    assert(game);
    assert(game_in_bounds(game, i_a, j_a));
    assert(game_in_bounds(game, i_b, j_b));

    ITER_NEIGHBORS(game, i_a, j_a, ni, nj)
    {
        if (game_is_unknown(game, ni, nj) && !are_neighbors(ni, nj, i_b, j_b))
            return false;
    }

    return true;
}

/**
 * Flags S(B) - S(A). That is: all neighbors of (i_b, j_b)
 * that are not neighbors of (i_a, j_a).
 */
[[nodiscard]] static bool flag_subset(game_t game[static 1], uint16_t i_a,
                                      uint16_t j_a, uint16_t i_b, uint16_t j_b)
{
    assert(game);
    assert(game_in_bounds(game, i_a, j_a));
    assert(game_in_bounds(game, i_b, j_b));
    assert(is_subset_of(game, i_a, j_a, i_b, j_b));

    bool retval = false;

    ITER_NEIGHBORS(game, i_b, j_b, ni, nj)
    {
        if (game_is_unknown(game, ni, nj) && !are_neighbors(ni, nj, i_a, j_a)) {
            assert(game_toggle_flag(game, ni, nj));
            retval = true;
        }
    }

    return retval;
}

/**
 * Reveals S(B) - S(A). That is: all neighbors of (i_b, j_b)
 * that are not neighbors of (i_a, j_a).
 */
[[nodiscard]] static bool reveal_subset(game_t game[static 1], uint16_t i_a,
                                        uint16_t j_a, uint16_t i_b,
                                        uint16_t j_b)
{

    assert(game);
    assert(game_in_bounds(game, i_a, j_a));
    assert(game_in_bounds(game, i_b, j_b));
    assert(is_subset_of(game, i_a, j_a, i_b, j_b));
    bool retval = false;

    ITER_NEIGHBORS(game, i_b, j_b, ni, nj)
    {
        if (game_is_unknown(game, ni, nj) && !are_neighbors(ni, nj, i_a, j_a)) {
            assert(game_reveal(game, ni, nj));
            retval = true;
        }
    }

    return retval;
}

[[nodiscard]] static bool solve_level_1(game_t game[static 1])
{
    assert(game);
    bool progressed = false;

    // let S(A) be the set of unknown neighbors of A
    // and m(A) be the number of mines left around A.
    //
    // this one follows the simplest rules:
    // - if m(A) = |S(A)|, then S(A) are all mines
    // - if m(A) = 0, then S(A) are all safe

    auto clue = GAME_UNFLAT(game, clue);

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            if (!is_boundary_clue(game, i, j))
                continue;

            uint8_t unknown = game_count_neighbors_unknown(game, i, j);
            uint8_t flags = game_count_neighbors_flagged(game, i, j);

            // rule 1: all mines
            if (unknown + flags == clue[i][j]) {
                if (flag_all_neighbors(game, i, j))
                    progressed = true;
            }

            // rule 2: all safe
            if (flags == clue[i][j]) {
                if (game_reveal(game, i, j))
                    progressed = true;
            }
        }
    }

    return progressed;
}

[[nodiscard]] static bool solve_level_2(game_t game[static 1])
{
    assert(game);
    bool progressed = false;

    // we look for pairs of boundary clues A, B
    // such that S(A) is a subset of S(B).
    // then, we deduce that S(B) - S(A) must contain m(B) - m(A) mines.

    auto clue = GAME_UNFLAT(game, clue);

    for (uint16_t i_a = 0; i_a < game->rows; ++i_a) {
        for (uint16_t j_a = 0; j_a < game->cols; ++j_a) {
            if (!is_boundary_clue(game, i_a, j_a))
                continue;

            // any possible pairing for A = (i, j) must be
            // within the 5x5 square centered around A
            ITER_NEIGHBORS_RADIUS(game, i_a, j_a, 2, i_b, j_b)
            {
                if (!is_boundary_clue(game, i_b, j_b) ||
                    !is_subset_of(game, i_a, j_a, i_b, j_b))
                    continue;

                // now we know that S(A) is a subset of S(B)
                uint8_t f_a = game_count_neighbors_flagged(game, i_a, j_a);
                uint8_t f_b = game_count_neighbors_flagged(game, i_b, j_b);
                if (f_a > clue[i_a][j_a] || f_b > clue[i_b][j_b])
                    continue;

                uint8_t m_a = clue[i_a][j_a] - f_a;
                uint8_t m_b = clue[i_b][j_b] - f_b;
                uint8_t n_a = game_count_neighbors_unknown(game, i_a, j_a);
                uint8_t n_b = game_count_neighbors_unknown(game, i_b, j_b);
                assert(n_a <= n_b);
                assert(m_a <= m_b);

                if (m_b - m_a == 0) {
                    // all of S(B) - S(A) are safe
                    if (reveal_subset(game, i_a, j_a, i_b, j_b))
                        progressed = true;
                } else if (m_b - m_a == n_b - n_a) {
                    // all of S(B) - S(A) are mines
                    if (flag_subset(game, i_a, j_a, i_b, j_b))
                        progressed = true;
                }
            }
        }
    }

    return progressed;
}

typedef enum : uint8_t {
    VAL_SAFE = 0,
    VAL_MINE = 1,
    VAL_UNDETERMINED = 2,
} cell_value_t;

typedef struct {
    bool solved;
    cell_value_t *vals;
} csp_result_t;

static void csp_callback(void *ctx, size_t n, uint8_t const vals[static n])
{
    assert(ctx);
    csp_result_t *res = ctx;

    if (!res->solved) {
        res->solved = true;
        for (size_t i = 0; i < n; ++i)
            res->vals[i] = vals[i];
    } else {
        for (size_t i = 0; i < n; ++i) {
            if (vals[i] != res->vals[i])
                res->vals[i] = VAL_UNDETERMINED;
        }
    }
}

[[nodiscard]] static bool solve_level_3(arena_t *arena, game_t game[static 1])
{
    static constexpr uint16_t NIL = (uint16_t)-1;
    assert(game);

    auto clue = GAME_UNFLAT(game, clue);
    auto revealed = GAME_UNFLAT(game, revealed);

    // this one formulates a CSP out of the known information
    // and attempts to solve it with csp.c

    int32_t rem = game_remaining_mines(game);
    if (rem <= 0)
        return false;

    auto cell_var = MAKE_2D(arena, (size_t)game->rows, game->cols, size_t);
    if (!cell_var)
        die("MAKE_2D");

    for (uint16_t i = 0; i < game->rows; ++i)
        for (uint16_t j = 0; j < game->cols; ++j)
            cell_var[i][j] = NIL;

    size_t cur_var = 0;

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            if (!is_boundary_clue(game, i, j))
                continue;

            ITER_NEIGHBORS(game, i, j, ni, nj)
            {
                if (game_is_unknown(game, ni, nj) && cell_var[ni][nj] == NIL)
                    cell_var[ni][nj] = cur_var++;
            }
        }
    }

    size_t n_vars = cur_var;
    csp_t *csp = csp_create(arena, n_vars);

    // We always consider the constraint
    // x_1 + x_2 + ... + x_n <= remaining mines.
    {
        auto cons = MAKE(arena, n_vars, size_t);
        if (!cons)
            die("MAKE");

        for (size_t i = 0; i < n_vars; ++i)
            cons[i] = i;

        if (!csp_add_constraint(csp, arena, n_vars, cons, rem, REL_LEQ))
            die("csp_add_constraint");
    }

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            if (!is_boundary_clue(game, i, j))
                continue;

            uint8_t flag_cnt = game_count_neighbors_flagged(game, i, j);
            if (clue[i][j] < flag_cnt)
                continue;

            size_t n = 0;
            size_t vars[MAX_NEIGHBORS] = {};

            ITER_NEIGHBORS(game, i, j, ni, nj)
            {
                if (cell_var[ni][nj] != NIL) {
                    assert(game_is_unknown(game, ni, nj));
                    vars[n++] = cell_var[ni][nj];
                }
            }

            size_t rhs = clue[i][j] - flag_cnt;
            if (!csp_add_constraint(csp, arena, n, vars, rhs, REL_EQ))
                die("csp_add_constraint");
        }
    }

    csp_result_t res = {
        .solved = false,
        .vals = MAKE(arena, n_vars, uint8_t),
    };
    if (!res.vals)
        die("MAKE");

    memset(res.vals, 0, n_vars * sizeof(res.vals));

    if (!csp_solve(csp, arena, &res, csp_callback))
        die("csp_solve");

    bool retval = true;

    if (!res.solved) {
        retval = false;
        goto cleanup;
    }

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            uint16_t x = cell_var[i][j];
            if (x == NIL)
                continue;

            if (res.vals[x] == VAL_SAFE) {
                if (!revealed[i][j])
                    assert(game_reveal(game, i, j));
            } else if (res.vals[x] == VAL_MINE) {
                assert(game_toggle_flag(game, i, j));
            }
        }
    }

cleanup:
    return retval;
}

bool solver_step(arena_t *arena, game_t game[static 1])
{
    assert(arena);
    assert(game->state != STATE_LOSE);
    return solve_level_1(game) || solve_level_2(game) ||
           solve_level_3(arena, game);
}
