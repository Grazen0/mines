#include "ansi.h"
#include "game.h"
#include "input.h"
#include <assert.h>
#include <signal.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    uint16_t rows;
    uint16_t cols;
} win_state_t;

typedef struct {
    size_t cur_i;
    size_t cur_j;
    uint16_t win_rows;
    uint16_t win_cols;
    bool render_pending;
} tui_t;

static void tui_update_win_size(tui_t tui[static 1])
{
    assert(tui);
    // update term size
    struct winsize ws = {};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    tui->win_rows = ws.ws_row;
    tui->win_cols = ws.ws_col;
}

static tui_t *tui_init(tui_t *tui, game_t const game[static 1])
{
    assert(game);

    if (!tui)
        return nullptr;

    *tui = (tui_t){};
    tui_update_win_size(tui);
    tui->cur_i = (game->rows - 1) / 2;
    tui->cur_j = (game->cols - 1) / 2;
    tui->render_pending = true;
    return tui;
}

static constexpr size_t CELL_WIDTH = 3;

static constexpr color_t COLOR_BG = {28, 32, 36};
static constexpr color_t COLOR_WALL = {39, 60, 66};
static constexpr color_t COLOR_FLAG = {247, 87, 87};
static constexpr color_t COLOR_FLAG_BG = {84, 40, 50};
static constexpr color_t COLOR_MINE = {30, 30, 30};
static constexpr color_t COLOR_MINE_BG = {238, 102, 102};

static constexpr color_t COLOR_CLUE[] = {
    { 56,  64,  72}, // .
    {124, 199, 255}, // 1
    {102, 194, 102}, // 2
    {255, 119, 136}, // 3
    {238, 136, 255}, // 4
    {221, 170,  34}, // 5
    {102, 204, 204}, // 6
    {153, 153, 153}, // 7
    {208, 216, 224}, // 8
};

// print's cell (i, j) at the current cursor position
static void print_cell(game_t const game[static 1], size_t i, size_t j,
                       bool highlight)
{
    assert(game);
    assert(i < game->rows);
    assert(i < game->rows);

    color_t bg = {};
    color_t fg = {};
    char c = ' ';

    size_t pos = (i * game->cols) + j;
    if (!game->revealed[pos] && (!game->is_mine[pos] || game->flagged[pos] ||
                                 game->state == STATE_PLAYING)) {
        bg = COLOR_WALL;
        fg = COLOR_FLAG;

        if (game->flagged[pos]) {
            bg = COLOR_FLAG_BG;
            c = 'F';
        }
    } else if (game->is_mine[pos]) {
        bg = COLOR_MINE_BG;
        fg = COLOR_MINE;
        c = 'X';
    } else {
        uint8_t clue = game->clue[pos];
        assert(clue <= 8);

        bg = COLOR_BG;
        fg = COLOR_CLUE[clue];

        if (clue == 0)
            c = '.';
        else
            c = (char)('0' + clue);
    }

    if (highlight) {
        bg = color_brighten(bg, 30);
        fg = color_brighten(fg, 20);
    }

    ansi_sgr(SGR_BOLD);
    ansi_sgr_bg_rgb(bg);
    ansi_sgr_fg_rgb(fg);
    printf(" %c ", c);
}

static void render(game_t const game[static 1], tui_t const tui[static 1])
{
    assert(game);
    assert(tui);

    ansi_clear(CLEAR_ALL);
    ansi_set_cursor(1, 1);

    if (tui->win_rows < game->rows || tui->win_cols < CELL_WIDTH * game->cols) {
        printf("Window too small!\r\n");
        fflush(stdout);
        return;
    }

    size_t top_row = (tui->win_rows - game->rows) / 2;
    size_t left_col = (tui->win_cols - CELL_WIDTH * game->cols) / 2;

    assert(top_row + game->rows <= tui->win_rows);
    assert(left_col + (CELL_WIDTH * game->cols) <= tui->win_cols);

    if (top_row >= 2) {
        ansi_set_cursor(1 + top_row - 2, 1 + left_col);
        ansi_sgr(SGR_RESET);
        printf(" Remaining: %2d ", (int)game->mine_cnt - (int)game->flag_cnt);
    }

    for (size_t i = 0; i < game->rows; ++i) {
        ansi_set_cursor(1 + top_row + i, 1 + left_col);
        for (size_t j = 0; j < game->cols; ++j) {
            bool highlight = (i == tui->cur_i && j == tui->cur_j);
            print_cell(game, i, j, highlight);
        }
    }

    ansi_sgr(SGR_RESET);
    fflush(stdout);
}

static volatile sig_atomic_t resize_pending;

static void sigwinch_handler(int sig)
{
    assert(sig == SIGWINCH);
    resize_pending = 1;
}

static void init_sigwinch_handler()
{
    struct sigaction act = {
        .sa_handler = sigwinch_handler,
        .sa_mask = {},
        .sa_flags = 0,
    };
    sigemptyset(&act.sa_mask);
    sigaction(SIGWINCH, &act, nullptr);
}

static void show_exit_message()
{
    ansi_sgr(SGR_RESET);
    ansi_clear(CLEAR_ALL);
    ansi_set_cursor(1, 1);
    printf("Goodbye...\n");
}

[[noreturn]] static void die(char const s[static 1])
{
    perror(s);
    exit(EXIT_FAILURE);
}

typedef struct {
    game_t game;
} app_t;

static void handle_key(game_t game[static 1], tui_t tui[static 1], char c)
{
    assert(game);
    assert(tui);
    assert(c);
    assert(c != -1);

    switch (c) {
        case KEY_UP:
            if (tui->cur_i == 0)
                tui->cur_i = game->rows - 1;
            else
                --tui->cur_i;
            tui->render_pending = true;
            break;
        case KEY_DOWN:
            tui->cur_i = (tui->cur_i + 1) % game->rows;
            tui->render_pending = true;
            break;
        case KEY_LEFT:
            if (tui->cur_j == 0)
                tui->cur_j = game->rows - 1;
            else
                --tui->cur_j;
            tui->render_pending = true;
            break;
        case KEY_RIGHT:
            tui->cur_j = (tui->cur_j + 1) % game->cols;
            tui->render_pending = true;
            break;
        case KEY_FLAG:
            if (game_toggle_flag(game, tui->cur_i, tui->cur_j))
                tui->render_pending = true;
            break;
        case KEY_ENTER:
        case KEY_REVEAL:
            if (game_reveal(game, tui->cur_i, tui->cur_j))
                tui->render_pending = true;
            break;
        default:
            break;
    }
}

static void run(game_t game[static 1], tui_t tui[static 1])
{
    assert(game);
    assert(tui);

    while (true) {
        if (resize_pending) {
            tui_update_win_size(tui);
            tui->render_pending = true;
            resize_pending = 0;
        }

        if (tui->render_pending) {
            render(game, tui);
            tui->render_pending = false;
        }

        char key = read_key();
        if (!key)
            continue;

        if (key == -1)
            die("read_key");

        if (key == CTRL_KEY('c') || key == CTRL_KEY('q'))
            break;

        handle_key(game, tui, key);
    }
}

int main()
{
    srand(time(nullptr));

    atexit(show_exit_message);

    enter_raw_mode();
    atexit(exit_raw_mode);

    ansi_hide_cursor();
    atexit(ansi_show_cursor);

    init_sigwinch_handler();

    game_t game = {};
    if (!game_init(&game, 15, 15, 25)) {
        perror("game_init");
        return EXIT_FAILURE;
    }

    tui_t tui = {};
    if (!tui_init(&tui, &game)) {
        perror("tui_init");
        return EXIT_FAILURE;
    }

    run(&game, &tui);

    game_deinit(&game);
    return EXIT_SUCCESS;
}
