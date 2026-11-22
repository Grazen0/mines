#include "tui.h"
#include "ansi.h"
#include "extra.h"
#include "game.h"
#include "input.h"
#include "memory.h"
#include "solver.h"
#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

#define UNFOLD(color) (color).r, (color).g, (color).b

static color_t color_brighten(color_t color, uint8_t add)
{
    return (color_t){
        MIN(color.r + add, 255),
        MIN(255, color.g + add),
        MIN(255, color.b + add),
    };
}

static constexpr color_t COLOR_BG = {28, 32, 36};
static constexpr color_t COLOR_WALL = {39, 60, 66};
static constexpr color_t COLOR_FLAG = {247, 87, 87};
static constexpr color_t COLOR_FLAG_BG = {84, 40, 50};
static constexpr color_t COLOR_MINE = {30, 30, 30};
static constexpr color_t COLOR_MINE_BG = {238, 102, 102};
static constexpr color_t COLOR_WIN_BG = {102, 194, 102};
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

#define STATUS_TEXT " remaining: %d "
#define GUIDE_TEXT_1 " move: hjkl |  flag: f  |  reveal: ␣ "
#define GUIDE_TEXT_2 " solve: c-s | reset: c-r  |  quit: c-q "
#define LOSE_TEXT " you lose! "
#define WIN_TEXT " you win! "
#define WIN_ALERT_TEXT " window is too small! "
#define WIN_ALERT_DETAILS_TEXT " required: %u rows x %u cols "

static constexpr size_t GAME_ARENA_SIZE = 32ULL * 1024;
static constexpr size_t SOLVER_ARENA_SIZE = 16ULL * 1024;

tui_t *tui_init(tui_t *tui, uint16_t rows, uint16_t cols, size_t mine_cnt)
{
    if (!tui)
        return nullptr;

    arena_t *game_arena = arena_create(GAME_ARENA_SIZE);
    if (!game_arena)
        goto err_cleanup_1;

    arena_t *solver_arena = arena_create(SOLVER_ARENA_SIZE);
    if (!solver_arena)
        goto err_cleanup_2;

    *tui = (tui_t){
        .cur_i = (rows - 1) / 2,
        .cur_j = (cols - 1) / 2,
        .game_arena = game_arena,
        .solver_arena = solver_arena,
    };
    if (!game_init(&tui->game, tui->game_arena, rows, cols, mine_cnt))
        goto err_cleanup_3;

    return tui;

err_cleanup_3:
    arena_destroy(solver_arena);
err_cleanup_2:
    arena_destroy(game_arena);
err_cleanup_1:
    return nullptr;
}

void tui_deinit(tui_t *tui)
{
    if (!tui)
        return;

    arena_destroy(tui->solver_arena);
    arena_destroy(tui->game_arena);
    *tui = (tui_t){};
}

static void tui_update_win_size(tui_t tui[static 1])
{
    assert(tui);

    // update win size
    struct winsize ws = {};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    tui->win_rows = ws.ws_row;
    tui->win_cols = ws.ws_col;
}

// prints cell (i, j) at the current cursor position
static void print_cell(tui_t const tui[static 1], size_t i, size_t j)
{
    assert(tui);
    assert(i < tui->game.rows);
    assert(j < tui->game.cols);

    auto revealed = GAME_UNFLAT(&tui->game, revealed);
    auto flagged = GAME_UNFLAT(&tui->game, flagged);
    auto is_mine = GAME_UNFLAT(&tui->game, is_mine);
    auto clue = GAME_UNFLAT(&tui->game, clue);

    color_t bg = {};
    color_t fg = {};
    char c = 0;

    if (tui->game.state == STATE_LOSE && is_mine[i][j]) {
        // draw as mine
        bg = COLOR_MINE_BG;
        fg = COLOR_MINE;
        c = 'X';
    } else if (!revealed[i][j]) {
        if (flagged[i][j]) {
            // draw as flag
            bg = COLOR_FLAG_BG;
            fg = COLOR_FLAG;
            c = 'F';
        } else {
            // draw as wall
            bg = COLOR_WALL;
            fg = COLOR_FLAG;
            c = ' ';
        }
    } else {
        // draw as clue
        assert(clue[i][j] <= 8);

        bg = COLOR_BG;
        fg = COLOR_CLUE[clue[i][j]];
        c = (char)('0' + clue[i][j]);
        if (c == '0')
            c = '.';
    }

    if (i == tui->cur_i && j == tui->cur_j) {
        // this cell is currently selected
        bg = color_brighten(bg, 30);
        fg = color_brighten(fg, 20);
    }

    printf(SGR_BOLD SGR_BG_RGB SGR_FG_RGB, UNFOLD(bg), UNFOLD(fg));
    printf(" %c ", c);
}

static bool print_centered(tui_t const tui[static 1], uint16_t row, bool forced,
                           char const s[static 1])
{
    assert(tui);
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

static constexpr uint16_t CELL_WIDTH = 3;

static void tui_render_win_alert(tui_t const tui[static 1])
{
    assert(tui);

    char *info = aprintf(WIN_ALERT_DETAILS_TEXT, tui->game.rows,
                         CELL_WIDTH * tui->game.cols);
    if (!info)
        die("aprintf");

    uint16_t mid = tui->win_rows / 2;

    printf(SGR_BOLD SGR_BG_BRIGHT_RED SGR_FG_BLACK);
    print_centered(tui, mid, true, WIN_ALERT_TEXT);
    print_centered(tui, mid + 1, true, info);
    printf(SGR_RESET);
    fflush(stdout);

    free(info);
}

static void tui_render_status(tui_t const tui[static 1], uint16_t top)
{
    assert(tui);

    if (top < 2)
        return;

    printf(SGR_RESET);
    char *status = aprintf(STATUS_TEXT, game_remaining_mines(&tui->game));
    if (!status)
        die("aprintf");

    printf(SET_CURSOR, 1 + top - 2, 1);
    printf(CLEAR_LINE);
    print_centered(tui, top - 2, false, status);
    free(status);
}

static void tui_render_game(tui_t const tui[static 1], uint16_t top,
                            uint16_t left)
{
    assert(tui);
    printf(SGR_RESET);

    for (uint16_t i = 0; i < tui->game.rows; ++i) {
        printf(SET_CURSOR, 1 + top + i, 1 + left);
        for (size_t j = 0; j < tui->game.cols; ++j)
            print_cell(tui, i, j);
    }
}

static void tui_render_guide(tui_t const tui[static 1], uint16_t top)
{
    printf(SGR_RESET);
    uint16_t guide_row = top + tui->game.rows + 1;

    if (guide_row < tui->win_rows) {
        printf(SET_CURSOR CLEAR_LINE, 1 + guide_row, 1);
        if (tui->game.state == STATE_WIN) {
            printf(SGR_BG_RGB SGR_FG_RGB, UNFOLD(COLOR_WIN_BG),
                   UNFOLD(COLOR_MINE));
            print_centered(tui, guide_row, false, WIN_TEXT);
        } else if (tui->game.state == STATE_LOSE) {
            printf(SGR_BG_RGB SGR_FG_RGB, UNFOLD(COLOR_MINE_BG),
                   UNFOLD(COLOR_MINE));
            print_centered(tui, guide_row, false, LOSE_TEXT);
        } else {
            print_centered(tui, guide_row, false, GUIDE_TEXT_1);
        }
    }

    if (guide_row + 2 < tui->win_rows) {
        printf(SGR_RESET);
        print_centered(tui, guide_row + 2, false, GUIDE_TEXT_2);
    }
}

static bool tui_can_be_rendered(tui_t const tui[static 1])
{
    return tui->game.rows <= tui->win_rows &&
           CELL_WIDTH * tui->game.cols < tui->win_cols;
}

static void tui_render(tui_t const tui[static 1])
{
    assert(tui);

    if (!tui_can_be_rendered(tui)) {
        tui_render_win_alert(tui);
        return;
    }

    uint16_t top = (tui->win_rows - tui->game.rows) / 2;
    uint16_t left = (tui->win_cols - CELL_WIDTH * tui->game.cols) / 2;

    tui_render_status(tui, top);
    tui_render_game(tui, top, left);
    tui_render_guide(tui, top);
    fflush(stdout);
}

static bool tui_move_cursor(tui_t tui[static 1], int16_t step_i, int16_t step_j)
{
    assert(tui);

    int32_t i_new = tui->cur_i + step_i;
    int32_t j_new = tui->cur_j + step_j;

    if (!game_in_bounds(&tui->game, i_new, j_new))
        return false;

    tui->cur_i = i_new;
    tui->cur_j = j_new;
    return true;
}

static void tui_handle_key(tui_t tui[static 1], char c)
{
    assert(tui);
    assert(c);
    assert(c != -1);

    if (!tui_can_be_rendered(tui))
        return;

    bool playing =
        tui->game.state == STATE_BEGIN || tui->game.state == STATE_PLAYING;

    switch (c) {
        case KEY_UP:
            if (tui_move_cursor(tui, -1, 0))
                tui->render_pending = true;
            break;
        case KEY_DOWN:
            if (tui_move_cursor(tui, 1, 0))
                tui->render_pending = true;
            break;
        case KEY_LEFT:
            if (tui_move_cursor(tui, 0, -1))
                tui->render_pending = true;
            break;
        case KEY_RIGHT:
            if (tui_move_cursor(tui, 0, 1))
                tui->render_pending = true;
            break;
        case KEY_FLAG:
        case KEY_FLAG_ALT:
            if (playing && game_toggle_flag(&tui->game, tui->cur_i, tui->cur_j))
                tui->render_pending = true;
            break;
        case KEY_REVEAL:
        case KEY_REVEAL_ALT:
            if (playing && game_reveal(&tui->game, tui->cur_i, tui->cur_j))
                tui->render_pending = true;
            break;
        case KEY_SOLVE:
            if (playing && solver_step(tui->solver_arena, &tui->game))
                tui->render_pending = true;

            arena_clear(tui->solver_arena);
            break;
        case KEY_RESET:
            game_reset(&tui->game);
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

static void cleanup()
{
    // note that this function runs after tui_deinit.

    printf(SHOW_CURSOR DISABLE_ALT_BUFFER SGR_RESET);
    exit_raw_mode();
    printf("goodbye!\n");
}

[[noreturn]] void tui_run(tui_t tui[static 1])
{
    assert(tui);

    // this function takes control over the program so that user code is forced
    // to only have a single tui object ever running.

    // this also allows us to control cleanup more directly right here.
    printf(HIDE_CURSOR ENABLE_ALT_BUFFER);
    enter_raw_mode();
    atexit(cleanup);

    // we use this to trigger an initial win size update + render
    resize_pending = 1;
    init_sigwinch_handler();

    while (true) {
        if (resize_pending) {
            tui_update_win_size(tui);
            printf(CLEAR_ALL);
            tui->render_pending = true;
            resize_pending = 0;
        }

        if (tui->render_pending) {
            tui_render(tui);
            tui->render_pending = false;
        }

        char key = read_key();
        if (key == -1)
            die("read_key");
        if (!key)
            continue;

        if (key == CTRL_KEY('c') || key == CTRL_KEY('q'))
            break;

        tui_handle_key(tui, key);
    }

    tui_deinit(tui); // self-destruct
    exit(EXIT_SUCCESS);
}
