#pragma once
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

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

[[nodiscard]] game_t *game_init(game_t *game, uint16_t rows, uint16_t cols,
                                size_t mine_cnt);

void game_deinit(game_t *game);

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

    size_t pos = (i * game->cols) + j;
    return !game->revealed[pos] && !game->flagged[pos];
}
