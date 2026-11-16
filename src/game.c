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
    size_t r = rand();
    return min + (r % (max - min));
}

static void shuffle_partial_sz(size_t n, size_t arr[static n], size_t lim)
{
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

bool game_toggle_flag(game_t game[static 1], size_t i, size_t j)
{
    assert(game);
    assert(i < game->rows);
    assert(j < game->cols);

    size_t pos = (i * game->cols) + j;
    if (game->revealed[pos])
        return false;

    if (game->flagged[pos]) {
        assert(game->flag_cnt > 0);
        game->flagged[pos] = false;
        --game->flag_cnt;
    } else {
        game->flagged[pos] = true;
        ++game->flag_cnt;
    }
    return true;
}

static void game_calc_clues(game_t game[static 1])
{
    memset(game->clue, 0, sizeof(*game->clue) * game->rows * game->cols);

    for (size_t i = 0; i < game->rows; ++i) {
        for (size_t j = 0; j < game->cols; ++j) {
            size_t pos = (i * game->cols) + j;
            if (!game->is_mine[pos])
                continue;

            for (int di = -1; di <= 1; ++di) {
                for (int dj = -1; dj <= 1; ++dj) {
                    if (i + di < game->rows && j + dj < game->cols) {
                        size_t npos = ((i + di) * game->cols) + j + dj;
                        ++game->clue[npos];
                    }
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

static size_t count_flags_around(game_t const game[static 1], size_t i,
                                 size_t j)
{
    assert(game);
    assert(i < game->rows);
    assert(j < game->cols);

    size_t count = 0;

    for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
            if (i + di < game->rows && j + dj < game->cols) {
                size_t npos = ((i + di) * game->cols) + j + dj;
                count += game->flagged[npos];
            }
        }
    }

    return count;
}

static void game_win(game_t game[static 1])
{
    for (uint16_t pos = 0; pos < game->rows * game->cols; ++pos) {
        if (game->is_mine[pos]) {
            assert(!game->revealed[pos]);
            game->flagged[pos] = true;
        } else {
            assert(game->revealed[pos]);
        }
    }

    game->state = STATE_WIN;
}

static bool game_reveal_inner(game_t game[static 1], size_t i, size_t j);

static void game_reveal_around(game_t game[static 1], size_t i, size_t j)
{
    assert(game);
    assert(i < game->rows);
    assert(j < game->cols);

    for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
            if (i + di < game->rows && j + dj < game->cols) {
                size_t npos = ((i + di) * game->cols) + j + dj;
                if (!game->is_mine[npos])
                    game_reveal_inner(game, i + di, j + dj);
            }
        }
    }
}

static bool game_reveal_inner(game_t game[static 1], size_t i, size_t j)
{
    assert(game);
    assert(i < game->rows);
    assert(j < game->cols);

    size_t pos = (i * game->cols) + j;
    if (game->revealed[pos] || game->flagged[pos])
        return false;

    game->revealed[pos] = true;
    ++game->reveal_cnt;

    if (game->is_mine[pos]) {
        game->state = STATE_LOSE;
    } else {
        assert(game->reveal_cnt + game->mine_cnt <= game->rows * game->cols);
        if (game->reveal_cnt + game->mine_cnt == game->rows * game->cols)
            game_win(game);
        else if (game->clue[pos] == 0)
            game_reveal_around(game, i, j);
    }

    return true;
}

bool game_reveal(game_t game[static 1], size_t i, size_t j)
{
    assert(game);
    assert(i < game->rows);
    assert(j < game->cols);
    assert(game->state != STATE_WIN);

    size_t pos = (i * game->cols) + j;
    if (game->flagged[pos])
        return false;

    if (game->revealed[pos]) {
        size_t flag_cnt = count_flags_around(game, i, j);
        if (flag_cnt != game->clue[pos])
            return false;

        for (int di = -1; di <= 1; ++di) {
            for (int dj = -1; dj <= 1; ++dj) {
                if (i + di < game->rows && j + dj < game->cols)
                    game_reveal_inner(game, i + di, j + dj);
            }
        }
        return true;
    }

    if (game->state == STATE_BEGIN) {
        game_gen_mines(game, i, j);
        assert(!game->is_mine[(i * game->cols) + j]);
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
