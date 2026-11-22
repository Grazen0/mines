#include "game.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

game_t *game_init(game_t *game, uint16_t rows, uint16_t cols, size_t mine_cnt)
{
    if (!game)
        return nullptr;

    if (game->mine_cnt > max_mine_cnt(rows, cols))
        return nullptr;

    uint8_t *clue = calloc((size_t)rows * cols, sizeof(*clue));
    if (!clue)
        goto err_cleanup_1;

    bool *is_mine = calloc((size_t)rows * cols, sizeof(*is_mine));
    if (!is_mine)
        goto err_cleanup_2;

    bool *revealed = calloc((size_t)rows * cols, sizeof(*revealed));
    if (!revealed)
        goto err_cleanup_3;

    bool *flagged = calloc((size_t)rows * cols, sizeof(*flagged));
    if (!flagged)
        goto err_cleanup_4;

    *game = (game_t){};
    game->rows = rows;
    game->cols = cols;
    game->mine_cnt = mine_cnt;
    game->clue = clue;
    game->is_mine = is_mine;
    game->revealed = revealed;
    game->flagged = flagged;
    game->state = STATE_BEGIN;
    return game;

err_cleanup_4:
    free(revealed);
err_cleanup_3:
    free(is_mine);
err_cleanup_2:
    free(clue);
err_cleanup_1:
    return nullptr;
}

void game_deinit(game_t *game)
{
    if (!game)
        return;

    free(game->flagged);
    free(game->revealed);
    free(game->is_mine);
    free(game->clue);
    *game = (game_t){};
}

bool game_toggle_flag(game_t game[static 1], uint16_t i, uint16_t j)
{
    assert(game);
    assert(i < game->rows);
    assert(j < game->cols);

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
    assert(i_safe < game->rows);
    assert(j_safe < game->cols);
    assert(game->state == STATE_BEGIN);

    size_t *candidates =
        calloc((size_t)game->rows * game->cols, sizeof(*candidates));
    assert(candidates);
    size_t n_candidates = 0;

    for (size_t i = 0; i < game->rows; ++i) {
        for (size_t j = 0; j < game->cols; ++j) {
            if (abs((int)i - (int)i_safe) > 1 || abs((int)j - (int)j_safe) > 1)
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
    assert(i < game->rows);
    assert(j < game->cols);

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
    assert(i < game->rows);
    assert(j < game->cols);

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
    assert(i < game->rows);
    assert(j < game->cols);

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
    assert(i < game->rows);
    assert(j < game->cols);

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
    assert(i < game->rows);
    assert(j < game->cols);

    uint8_t count = 0;

    ITER_NEIGHBORS(game, i, j, ni, nj)
    {
        count += game_is_unknown(game, ni, nj);
    }

    return count;
}
