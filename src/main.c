#include "ansi.h"
#include "color.h"
#include "extra.h"
#include "game.h"
#include "input.h"
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
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

#define STATUS_TEXT "remaining: %d"
#define GUIDE_TEXT_1 "move: hjkl  |  flag: f  |  reveal: ␣"
#define GUIDE_TEXT_2 "use shift to move faster"
#define WIN_TOO_SMALL_TEXT " window is too small! "
#define WIN_SIZE_REQUIRED_TEXT " required: %u rows x %u cols "

[[noreturn]] static void die(char const s[static 1])
{
    perror(s);
    exit(EXIT_FAILURE);
}
typedef struct {
    uint16_t rows;
    uint16_t cols;
} win_state_t;

static constexpr uint16_t CELL_WIDTH = 3;

typedef struct {
    uint16_t cur_i;
    uint16_t cur_j;
    uint16_t win_rows;
    uint16_t win_cols;
    bool render_pending;
} tui_t;

static void tui_update_win_size(tui_t tui[static 1])
{
    assert(tui);

    // update win size
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
    tui->cur_i = (game->rows - 1) / 2;
    tui->cur_j = (game->cols - 1) / 2;
    tui->render_pending = true;
    return tui;
}

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

    printf(SGR_BOLD SGR_BG_RGB SGR_FG_RGB, UNFOLD(bg), UNFOLD(fg));
    printf(" %c ", c);
}

static bool print_centered(tui_t const tui[static 1], uint16_t row, bool forced,
                           char const s[static 1])
{
    assert(s);
    size_t len = strlen(s);
    uint16_t col = 0;

    if (tui->win_cols < len) {
        if (!forced)
            return false;

        col = 0;
    } else {
        col = (tui->win_cols - len + 1) / 2;
    }

    printf(SET_CURSOR, 1 + row, 1 + col);
    fputs(s, stdout);
    return true;
}

static void render_win_size_message(game_t const game[static 1],
                                    tui_t const tui[static 1])
{
    char *info =
        aprintf(WIN_SIZE_REQUIRED_TEXT, game->rows, CELL_WIDTH * game->cols);
    if (!info)
        die("aprintf");

    uint16_t mid = tui->win_rows / 2;

    printf(SGR_BOLD SGR_BG_BRIGHT_RED SGR_FG_BLACK);
    print_centered(tui, mid, true, WIN_TOO_SMALL_TEXT);
    print_centered(tui, mid + 1, true, info);
    printf(SGR_RESET);
    fflush(stdout);

    free(info);
}

static void render_status(game_t const game[static 1],
                          tui_t const tui[static 1], uint16_t top)
{
    assert(game);
    assert(tui);
    printf(SGR_RESET);

    if (top >= 2) {
        int rem = (int)game->mine_cnt - (int)game->flag_cnt;
        char *status = aprintf(STATUS_TEXT, rem);
        if (!status)
            die("aprintf");

        printf(SET_CURSOR, 1 + top - 2, 1);
        printf(CLEAR_LINE);
        print_centered(tui, top - 2, false, status);
        free(status);
    }
}

static void render_field(game_t const game[static 1], tui_t const tui[static 1],
                         uint16_t top, uint16_t left)
{
    assert(game);
    assert(tui);
    printf(SGR_RESET);

    for (uint16_t i = 0; i < game->rows; ++i) {
        printf(SET_CURSOR, 1 + top + i, 1 + left);
        for (size_t j = 0; j < game->cols; ++j) {
            bool highlight = (i == tui->cur_i && j == tui->cur_j);
            print_cell(game, i, j, highlight);
        }
    }
}

static void render_guide(game_t const game[static 1], tui_t const tui[static 1],
                         uint16_t top)
{

    uint16_t guide_row = top + game->rows + 1;

    if (guide_row < tui->win_rows) {
        printf(SGR_RESET);
        print_centered(tui, guide_row, false, GUIDE_TEXT_1);
        if (guide_row + 2 < tui->win_rows)
            print_centered(tui, guide_row + 2, false, GUIDE_TEXT_2);
    }
}

static void render(game_t const game[static 1], tui_t const tui[static 1])
{
    assert(game);
    assert(tui);

    if (tui->win_rows < game->rows || tui->win_cols < CELL_WIDTH * game->cols) {
        render_win_size_message(game, tui);
        return;
    }

    uint16_t top = (tui->win_rows - game->rows) / 2;
    uint16_t left = (tui->win_cols - CELL_WIDTH * game->cols) / 2;

    render_status(game, tui, top);
    render_field(game, tui, top, left);
    render_guide(game, tui, top);
    fflush(stdout);
}

static void show_exit_message()
{
    printf(SGR_RESET CLEAR_ALL SET_CURSOR, 1, 1);
    printf("goodbye!\n");
}

static bool tui_move_cursor(tui_t tui[static 1], game_t const game[static 1],
                            int16_t step_i, int16_t step_j, bool fast)
{
    assert(tui);

    size_t pos = (tui->cur_i * game->cols) + tui->cur_j;
    int32_t i_new = tui->cur_i + step_i;
    int32_t j_new = tui->cur_j + step_j;
    size_t pos_new = (i_new * game->cols) + j_new;

    if (i_new < 0 || i_new >= game->rows || j_new < 0 || j_new >= game->cols)
        return false;

    tui->cur_i = i_new;
    tui->cur_j = j_new;

    // end move instantly if we moved across a wall border
    if (!fast || game->revealed[pos] != game->revealed[pos_new])
        return true;

    // otherwise, walk till we hit a wall border
    while (true) {
        i_new = tui->cur_i + step_i;
        j_new = tui->cur_j + step_j;
        if (i_new < 0 || i_new >= game->rows || j_new < 0 ||
            j_new >= game->cols)
            break;

        size_t pos_new = (i_new * game->cols) + j_new;
        if (game->revealed[pos] != game->revealed[pos_new])
            break;

        tui->cur_i = i_new;
        tui->cur_j = j_new;
    }

    return true;
}

static void handle_key(game_t game[static 1], tui_t tui[static 1], char c)
{
    assert(game);
    assert(tui);
    assert(c);
    assert(c != -1);

    switch (c) {
        case KEY_UP:
            if (tui_move_cursor(tui, game, -1, 0, false))
                tui->render_pending = true;
            break;
        case KEY_DOWN:
            if (tui_move_cursor(tui, game, 1, 0, false))
                tui->render_pending = true;
            break;
        case KEY_LEFT:
            if (tui_move_cursor(tui, game, 0, -1, false))
                tui->render_pending = true;
            break;
        case KEY_RIGHT:
            if (tui_move_cursor(tui, game, 0, 1, false))
                tui->render_pending = true;
            break;
        case KEY_UP_FAST:
            if (tui_move_cursor(tui, game, -1, 0, true))
                tui->render_pending = true;
            break;
        case KEY_DOWN_FAST:
            if (tui_move_cursor(tui, game, 1, 0, true))
                tui->render_pending = true;
            break;
        case KEY_LEFT_FAST:
            if (tui_move_cursor(tui, game, 0, -1, true))
                tui->render_pending = true;
            break;
        case KEY_RIGHT_FAST:
            if (tui_move_cursor(tui, game, 0, 1, true))
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
        case KEY_RESET:
            game_reset(game);
            tui->render_pending = true;
            break;
        default:
            break;
    }
}

static volatile sig_atomic_t resize_pending = 0;

static void sigwinch_handler(int sig)
{
    assert(sig == SIGWINCH);
    resize_pending = 1;
}

static void init_sigwinch_handler()
{
    struct sigaction act = {
        .sa_handler = sigwinch_handler,
        .sa_flags = 0,
    };
    sigemptyset(&act.sa_mask);
    sigaction(SIGWINCH, &act, nullptr);
}

static void run(game_t game[static 1], tui_t tui[static 1])
{
    assert(game);
    assert(tui);

    resize_pending = 1;

    while (true) {
        if (resize_pending) {
            tui_update_win_size(tui);
            printf(CLEAR_ALL);
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

static void show_cursor()
{
    printf(SHOW_CURSOR);
}

int main()
{
    srand(time(nullptr));

    atexit(show_exit_message);

    enter_raw_mode();
    atexit(exit_raw_mode);

    printf(HIDE_CURSOR);
    atexit(show_cursor);

    init_sigwinch_handler();

    game_t game = {};
    if (!game_init(&game, 15, 15, 25))
        die("game_init");

    tui_t tui = {};
    if (!tui_init(&tui, &game))
        die("tui_init");

    run(&game, &tui);

    game_deinit(&game);
    return EXIT_SUCCESS;
}
