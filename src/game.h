#pragma once
#include <stddef.h>
#include <stdint.h>

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
    size_t rows;
    size_t cols;
    size_t mine_cnt;
    size_t flag_cnt;
    game_state_t state;
} game_t;

game_t *game_init(game_t *game, size_t rows, size_t cols, size_t mine_cnt);

void game_deinit(game_t *game);

bool game_toggle_flag(game_t game[static 1], size_t i, size_t j);

bool game_reveal(game_t game[static 1], size_t i, size_t j);
