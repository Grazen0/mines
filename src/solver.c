#include "solver.h"
#include "game.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// solvers based on https://minesweeperblast.com/minesweeper-solver-algorithms/

[[nodiscard]] static bool are_neighbors(uint16_t i1, uint16_t j1, uint16_t i2,
                                        uint16_t j2)
{
    assert(i1 != i2 || j1 != j2);
    return abs(i1 - i2) <= 1 && abs(j1 - j2) <= 1;
}

[[nodiscard]] static bool is_boundary_clue(game_t const game[static 1],
                                           uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    size_t pos = (i * game->cols) + j;
    if (!game->revealed[pos])
        return false;

    for (int32_t di = -1; di <= 1; ++di) {
        for (int32_t dj = -1; dj <= 1; ++dj) {
            if ((di == 0 && dj == 0) || !game_in_bounds(game, i + di, j + dj))
                continue;

            size_t npos = ((i + di) * game->cols) + j + dj;
            if (!game->revealed[npos] && !game->flagged[npos])
                return true;
        }
    }
    return false;
}

[[nodiscard]] static bool flag_all_neighbors(game_t game[static 1], uint16_t i,
                                             uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));
    bool retval = false;

    for (int32_t di = -1; di <= 1; ++di) {
        for (int32_t dj = -1; dj <= 1; ++dj) {
            if ((di == 0 && dj == 0) || !game_in_bounds(game, i + di, j + dj))
                continue;

            uint16_t ni = i + di;
            uint16_t nj = j + dj;
            if (game_is_unknown(game, ni, nj)) {
                assert(game_toggle_flag(game, ni, nj));
                retval = true;
            }
        }
    }

    return retval;
}

[[nodiscard]] static bool solve_level_1(game_t game[static 1])
{
    assert(game);
    bool retval = false;

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            if (!is_boundary_clue(game, i, j))
                continue;

            size_t pos = (i * game->cols) + j;
            uint8_t covered = game_count_neighbors_unknown(game, i, j);
            uint8_t flagged = game_count_neighbors_flagged(game, i, j);
            uint8_t n = game->clue[pos];

            // rule 1: all mines
            if (covered + flagged == n)
                retval = flag_all_neighbors(game, i, j) || retval;

            // rule 2: all safe
            if (flagged == n)
                retval = game_reveal(game, i, j) || retval;
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

    for (int32_t di = -1; di <= 1; ++di) {
        for (int32_t dj = -1; dj <= 1; ++dj) {
            if ((di == 0 && dj == 0) ||
                !game_in_bounds(game, i_a + di, j_a + dj))
                continue;

            uint16_t ni = i_a + di;
            uint16_t nj = j_a + dj;
            if (game_is_unknown(game, ni, nj) &&
                !are_neighbors(ni, nj, i_b, j_b))
                return false;
        }
    }

    return true;
}

// flags all neighbors of (i_b, j_b) that are not neighbors of (i_a, j_a)
[[nodiscard]] static bool flag_subset(game_t game[static 1], uint16_t i_a,
                                      uint16_t j_a, uint16_t i_b, uint16_t j_b)
{
    assert(game);
    assert(game_in_bounds(game, i_a, j_a));
    assert(game_in_bounds(game, i_b, j_b));
    assert(is_subset_of(game, i_a, j_a, i_b, j_b));

    bool retval = false;

    for (int32_t di = -1; di <= 1; ++di) {
        for (int32_t dj = -1; dj <= 1; ++dj) {
            if ((di == 0 && dj == 0) ||
                !game_in_bounds(game, i_b + di, j_b + dj))
                continue;

            uint16_t ni = i_b + di;
            uint16_t nj = j_b + dj;
            if (game_is_unknown(game, ni, nj) &&
                !are_neighbors(ni, nj, i_a, j_a)) {
                assert(game_toggle_flag(game, ni, nj));
                retval = true;
            }
        }
    }

    return retval;
}

[[nodiscard]] static bool reveal_subset(game_t game[static 1], uint16_t i_a,
                                        uint16_t j_a, uint16_t i_b,
                                        uint16_t j_b)
{

    assert(game);
    assert(game_in_bounds(game, i_a, j_a));
    assert(game_in_bounds(game, i_b, j_b));
    assert(is_subset_of(game, i_a, j_a, i_b, j_b));

    bool retval = false;

    for (int32_t di = -1; di <= 1; ++di) {
        for (int32_t dj = -1; dj <= 1; ++dj) {
            if ((di == 0 && dj == 0) ||
                !game_in_bounds(game, i_b + di, j_b + dj))
                continue;

            uint16_t ni = i_b + di;
            uint16_t nj = j_b + dj;
            if (game_is_unknown(game, ni, nj) &&
                !are_neighbors(ni, nj, i_a, j_a)) {
                assert(game_reveal(game, ni, nj));
                retval = true;
            }
        }
    }

    return retval;
}

[[nodiscard]] static bool solve_level_2(game_t game[static 1])
{
    assert(game);
    bool retval = false;

    // let S(A) be the set of unknown neighbors of A
    // and m(A) be the number of mines around A.
    //
    // we look for pairs of boundary clues A, B
    // such that S(A) is a subset of S(B).
    // then, we deduce that S(B) - S(A) must contain m(B) - m(A) mines.

    for (uint16_t i_a = 0; i_a < game->rows; ++i_a) {
        for (uint16_t j_a = 0; j_a < game->cols; ++j_a) {
            size_t pos_a = (i_a * game->cols) + j_a;
            if (!game->revealed[pos_a])
                continue;

            if (!is_boundary_clue(game, i_a, j_a))
                continue;

            // any possible pairing for A = (i, j) must be
            // within the 5x5 square centered around A
            for (int32_t di = -2; di <= 2; ++di) {
                for (int32_t dj = -2; dj <= 2; ++dj) {
                    if ((di == 0 && dj == 0) ||
                        !game_in_bounds(game, i_a + di, j_a + dj))
                        continue;

                    uint16_t i_b = i_a + di;
                    uint16_t j_b = j_a + dj;
                    size_t pos_b = (i_b * game->cols) + j_b;

                    if (!is_boundary_clue(game, i_b, j_b) ||
                        !is_subset_of(game, i_a, j_a, i_b, j_b))
                        continue;

                    // now we know that S(A) is a subset of S(B)
                    uint8_t f_a = game_count_neighbors_flagged(game, i_a, j_a);
                    uint8_t f_b = game_count_neighbors_flagged(game, i_b, j_b);
                    if (f_a > game->clue[pos_a] || f_b > game->clue[pos_b])
                        continue;

                    uint8_t m_a = game->clue[pos_a] - f_a;
                    uint8_t m_b = game->clue[pos_b] - f_b;
                    uint8_t n_a = game_count_neighbors_unknown(game, i_a, j_a);
                    uint8_t n_b = game_count_neighbors_unknown(game, i_b, j_b);
                    assert(n_a <= n_b);
                    // assert(ma <= mb);

                    if (m_b == m_a) {
                        // all extra cells (S(B) - S(A)) are safe
                        retval =
                            reveal_subset(game, i_a, j_a, i_b, j_b) || retval;
                    } else if (m_b + n_a == n_b + m_a) {
                        // all extra cells (S(B) - S(A)) are mines
                        retval =
                            flag_subset(game, i_a, j_a, i_b, j_b) || retval;
                    }
                }
            }
        }
    }

    return retval;
}

bool solver_step(game_t game[static 1])
{
    assert(game->state != STATE_LOSE);
    return solve_level_1(game) || solve_level_2(game);
}
