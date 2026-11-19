#include "solver.h"
#include "game.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// based on https://minesweeperblast.com/minesweeper-solver-algorithms/

static bool flag_all_neighbors(game_t game[static 1], uint16_t i, uint16_t j)
{
    bool retval = false;

    for (int32_t di = -1; di <= 1; ++di) {
        for (int32_t dj = -1; dj <= 1; ++dj) {
            if ((di == 0 && dj == 0) || !game_in_bounds(game, i + di, j + dj))
                continue;

            size_t npos = ((i + di) * game->cols) + j + dj;
            if (!game->revealed[npos] && !game->flagged[npos]) {
                assert(game_toggle_flag(game, i + di, j + dj));
                retval = true;
            }
        }
    }

    return retval;
}

static bool solve_level_1(game_t game[static 1])
{
    assert(game->state != STATE_LOSE);
    bool retval = false;

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            size_t pos = (i * game->cols) + j;
            if (!game->revealed[pos])
                continue;

            uint8_t rem = game_count_neighbors_unrevealed(game, i, j);
            uint8_t flags = game_count_neighbors_flagged(game, i, j);
            uint8_t clue = game->clue[pos];

            // rule 1: all mines
            if (rem == clue)
                retval = flag_all_neighbors(game, i, j) || retval;

            // rule 2: all safe
            if (flags == clue)
                retval = game_reveal(game, i, j) || retval;
        }
    }

    return retval;
}

bool solver_step(game_t game[static 1])
{
    assert(game->state != STATE_LOSE);
    return solve_level_1(game);
}
