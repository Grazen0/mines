#include "game.h"
#include "memory.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint16_t max_mine_cnt(uint16_t rows, uint16_t cols)
{
    if (rows * cols <= 9)
        return 0;

    return (rows * cols) - 9;
}

static size_t rand_sz(size_t min, size_t max) // [min, max)
{
    assert(min < max);
    double r = rand() / ((double)RAND_MAX + 1);
    return min + (size_t)(r * (double)(max - min));
}

static void shuffle_partial_sz(size_t n, size_t arr[static n], size_t lim)
{
    assert(arr);
    assert(lim <= n);

    for (size_t i = 0; i < lim; ++i) {
        size_t j = rand_sz(i, n);
        if (i != j) {
            size_t tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
        }
    }
}

game_t *game_init(game_t *game, arena_t *arena, uint16_t rows, uint16_t cols,
                  size_t mine_cnt)
{
    if (!game)
        return nullptr;

    if (!arena || game->mine_cnt > max_mine_cnt(rows, cols)) {
        errno = EINVAL;
        return nullptr;
    }

    auto clue = MAKE(arena, (size_t)rows * cols, uint8_t);
    if (!clue)
        return nullptr;

    auto is_mine = MAKE(arena, (size_t)rows * cols, bool);
    if (!is_mine)
        return nullptr;

    auto revealed = MAKE(arena, (size_t)rows * cols, bool);
    if (!revealed)
        return nullptr;

    auto flagged = MAKE(arena, (size_t)rows * cols, bool);
    if (!flagged)
        return nullptr;

    memset(clue, 0, (size_t)rows * cols * sizeof(*clue));
    memset(is_mine, 0, (size_t)rows * cols * sizeof(*is_mine));
    memset(revealed, 0, (size_t)rows * cols * sizeof(*revealed));
    memset(flagged, 0, (size_t)rows * cols * sizeof(*flagged));

    *game = (game_t){
        .rows = rows,
        .cols = cols,
        .mine_cnt = mine_cnt,
        .clue = clue,
        .is_mine = is_mine,
        .revealed = revealed,
        .flagged = flagged,
        .state = STATE_BEGIN,
    };
    return game;
}

bool game_toggle_flag(game_t game[static 1], uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto revealed = GAME_UNFLAT(game, revealed);
    auto flagged = GAME_UNFLAT(game, flagged);

    if (revealed[i][j])
        return false;

    if (flagged[i][j]) {
        assert(game->flag_cnt > 0);
        flagged[i][j] = false;
        --game->flag_cnt;
    } else {
        flagged[i][j] = true;
        ++game->flag_cnt;
    }
    return true;
}

static void game_calc_clues(game_t game[static 1])
{
    memset(game->clue, 0, sizeof(*game->clue) * game->rows * game->cols);

    auto is_mine = GAME_UNFLAT(game, is_mine);
    auto clue = GAME_UNFLAT(game, clue);

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            if (is_mine[i][j]) {
                ITER_NEIGHBORS(game, i, j, ni, nj)
                {
                    ++clue[ni][nj];
                }
            }
        }
    }
}

static void game_gen_mines(game_t game[static 1], size_t i_safe, size_t j_safe)
{
    assert(game);
    assert(game_in_bounds(game, i_safe, j_safe));
    assert(game->state == STATE_BEGIN);

    size_t *candidates =
        calloc((size_t)game->rows * game->cols, sizeof(*candidates));
    assert(candidates);
    size_t n_candidates = 0;

    for (size_t i = 0; i < game->rows; ++i) {
        for (size_t j = 0; j < game->cols; ++j) {
            if (i == i_safe && j == j_safe)
                continue;

            if (!are_neighbors(i, j, i_safe, j_safe))
                candidates[n_candidates++] = (i * game->cols) + j;
        }
    }

    shuffle_partial_sz(n_candidates, candidates, game->mine_cnt);

    for (size_t i = 0; i < game->mine_cnt; ++i)
        game->is_mine[candidates[i]] = true;

    game_calc_clues(game);
    free(candidates);
}

static void game_win(game_t game[static 1])
{
    assert(game);

    auto revealed = GAME_UNFLAT(game, revealed);
    auto is_mine = GAME_UNFLAT(game, is_mine);
    auto flagged = GAME_UNFLAT(game, flagged);

    for (uint16_t i = 0; i < game->rows; ++i) {
        for (uint16_t j = 0; j < game->cols; ++j) {
            if (is_mine[i][j]) {
                if (!flagged[i][j])
                    assert(game_toggle_flag(game, i, j));
            } else {
                assert(revealed[i][j]);
            }
        }
    }

    game->state = STATE_WIN;
}

static bool game_reveal_inner(game_t game[static 1], uint16_t i, uint16_t j);

static void game_reveal_around(game_t game[static 1], uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto flagged = GAME_UNFLAT(game, flagged);

    ITER_NEIGHBORS(game, i, j, ni, nj)
    {
        if (!flagged[ni][nj])
            game_reveal_inner(game, ni, nj);
    }
}

static bool game_reveal_inner(game_t game[static 1], uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto revealed = GAME_UNFLAT(game, revealed);
    auto flagged = GAME_UNFLAT(game, flagged);
    auto is_mine = GAME_UNFLAT(game, is_mine);
    auto clue = GAME_UNFLAT(game, clue);

    if (revealed[i][j] || flagged[i][j])
        return false;

    revealed[i][j] = true;
    ++game->reveal_cnt;

    if (is_mine[i][j]) {
        game->state = STATE_LOSE;
    } else {
        assert(game->reveal_cnt + game->mine_cnt <= game->rows * game->cols);

        if (game->reveal_cnt + game->mine_cnt == game->rows * game->cols)
            game_win(game);
        else if (clue[i][j] == 0)
            game_reveal_around(game, i, j);
    }

    return true;
}

bool game_reveal(game_t game[static 1], uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto revealed = GAME_UNFLAT(game, revealed);
    auto flagged = GAME_UNFLAT(game, flagged);
    auto is_mine = GAME_UNFLAT(game, is_mine);
    auto clue = GAME_UNFLAT(game, clue);

    if (game->state == STATE_WIN || game->state == STATE_LOSE)
        return false;

    if (flagged[i][j])
        return false;

    if (revealed[i][j]) {
        // auto-reveal around completed clues
        size_t flag_cnt = game_count_neighbors_flagged(game, i, j);
        if (flag_cnt != clue[i][j])
            return false;

        ITER_NEIGHBORS(game, i, j, ni, nj)
        {
            if (game_in_bounds(game, ni, nj))
                game_reveal_inner(game, ni, nj);
        }
        return true;
    }

    if (game->state == STATE_BEGIN) {
        game_gen_mines(game, i, j);
        assert(!is_mine[i][j]);
        game->state = STATE_PLAYING;
    }

    return game_reveal_inner(game, i, j);
}

void game_reset(game_t game[static 1])
{
    assert(game);

    memset(game->clue, 0, sizeof(*game->clue) * game->rows * game->cols);
    memset(game->is_mine, 0, sizeof(*game->is_mine) * game->rows * game->cols);
    memset(game->revealed, 0,
           sizeof(*game->revealed) * game->rows * game->cols);
    memset(game->flagged, 0, sizeof(*game->flagged) * game->rows * game->cols);
    game->flag_cnt = 0;
    game->reveal_cnt = 0;
    game->state = STATE_BEGIN;
}

uint8_t game_count_neighbors_flagged(game_t const game[static 1], uint16_t i,
                                     uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto flagged = GAME_UNFLAT(game, flagged);
    uint8_t count = 0;

    ITER_NEIGHBORS(game, i, j, ni, nj)
    {
        count += flagged[ni][nj];
    }

    return count;
}

uint8_t game_count_neighbors_unknown(game_t const game[static 1], uint16_t i,
                                     uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    uint8_t count = 0;

    ITER_NEIGHBORS(game, i, j, ni, nj)
    {
        count += game_is_unknown(game, ni, nj);
    }

    return count;
}
