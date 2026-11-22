#pragma once
#include "game.h"
#include "memory.h"
#include <stdint.h>

typedef struct {
    game_t game;
    uint16_t cur_i;
    uint16_t cur_j;
    uint16_t win_rows;
    uint16_t win_cols;
    bool render_pending;
    arena_t *game_arena;
    arena_t *solver_arena;
} tui_t;

[[nodiscard]] tui_t *tui_init(tui_t *tui, uint16_t rows, uint16_t cols,
                              size_t mine_cnt);

void tui_deinit(tui_t *tui);

void tui_run(tui_t tui[static 1]);
