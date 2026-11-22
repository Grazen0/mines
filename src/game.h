#pragma once
#include "memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static constexpr size_t MAX_NEIGHBORS = 8;

[[nodiscard]] uint16_t max_mine_cnt(uint16_t rows, uint16_t cols);

typedef enum : uint8_t {
    STATE_BEGIN,
    STATE_PLAYING,
    STATE_WIN,
    STATE_LOSE,
} game_state_t;

typedef struct {
    uint8_t *clue; // rows x cols
    bool *is_mine; // rows x cols
    bool *revealed; // rows x cols
    bool *flagged; // rows x cols
    uint16_t rows;
    uint16_t cols;
    uint16_t mine_cnt;
    uint16_t flag_cnt;
    uint16_t reveal_cnt;
    game_state_t state;
} game_t;

#define GAME_UNFLAT(game, field) \
    ((typeof (*(game)->field)(*)[(game)->cols])(game)->field)
#define GAME_UNFLAT_CONST(game, field) \
    ((typeof(*(game)->field) const(*)[(game)->cols])(game)->field)

#define ITER_NEIGHBORS_RADIUS(game, i, j, r, ni, nj)    \
    for (int32_t _di = -(r); _di <= (r); ++_di)         \
        for (int32_t _dj = -(r); _dj <= (r); ++_dj)     \
            if ((_di != 0 || _dj != 0) &&               \
                game_in_bounds(game, i + _di, j + _dj)) \
                for (uint16_t ni = i + _di, nj = j + _dj, _go = 1; _go; _go = 0)

#define ITER_NEIGHBORS(game, i, j, ni, nj) \
    ITER_NEIGHBORS_RADIUS(game, i, j, 1, ni, nj)

[[nodiscard]] game_t *game_init(game_t *game, arena_t *arena, uint16_t rows,
                                uint16_t cols, size_t mine_cnt);

bool game_toggle_flag(game_t game[static 1], uint16_t i, uint16_t j);

bool game_reveal(game_t game[static 1], uint16_t i, uint16_t j);

void game_reset(game_t game[static 1]);

[[nodiscard]] uint8_t game_count_neighbors_unknown(game_t const game[static 1],
                                                   uint16_t i, uint16_t j);

[[nodiscard]] uint8_t game_count_neighbors_flagged(game_t const game[static 1],
                                                   uint16_t i, uint16_t j);

[[nodiscard]] static inline bool game_in_bounds(game_t const game[static 1],
                                                int32_t i, int32_t j)
{
    assert(game);
    return i >= 0 && i < game->rows && j >= 0 && j < game->cols;
}

[[nodiscard]] static inline bool game_is_unknown(game_t const game[static 1],
                                                 uint16_t i, uint16_t j)
{
    assert(game);
    assert(game_in_bounds(game, i, j));

    auto revealed = GAME_UNFLAT(game, revealed);
    auto flagged = GAME_UNFLAT(game, flagged);

    return !revealed[i][j] && !flagged[i][j];
}

[[nodiscard]] static inline int32_t
game_remaining_mines(game_t const game[static 1])
{
    assert(game);
    return (int32_t)game->mine_cnt - game->flag_cnt;
}

[[nodiscard]] static inline bool are_neighbors(uint16_t i1, uint16_t j1,
                                               uint16_t i2, uint16_t j2)
{
    assert(i1 != i2 || j1 != j2);
    return abs((int)i1 - i2) <= 1 && abs((int)j1 - j2) <= 1;
}
