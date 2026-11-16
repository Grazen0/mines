#include "extra.h"
#include "tui.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const struct option long_options[] = {
    { "help",       no_argument, nullptr, 'h'},
    {"mines", required_argument, nullptr, 'm'},
    {nullptr,                 0, nullptr,   0},
};

typedef struct {
    uint16_t rows;
    uint16_t cols;
    size_t mine_cnt;
} args_t;

#define EXE_NAME "mines"
#define USAGE                                             \
    "Usage: " EXE_NAME " [options...] [rows] [cols]   \n" \
    "A simple TUI Minesweeper game made in C.         \n" \
    "                                                 \n" \
    "Arguments:                                       \n" \
    "  rows   Field height (defaults to 16)           \n" \
    "  cols   Field width  (defaults to rows)         \n" \
    "                                                 \n" \
    "Flags:                                           \n" \
    "  -h, --help   show this help message            \n" \
    "  -m, --mines  number of mines to place          \n" \
    "               (default: 16%% of rows x cols)    \n" \
    "                                                 \n" \
    "Examples:                                        \n" \
    "  " EXE_NAME "           16 rows x 16 cols       \n" \
    "  " EXE_NAME " 10        10 rows x 10 cols       \n" \
    "  " EXE_NAME " 10 8      10 rows x 8 cols        \n"

static constexpr uint16_t DEFAULT_ROWS = 16;
static constexpr double DEFAULT_MINE_DENSITY = 0.16;

static void parse_args(int argc, char *argv[static argc + 1], args_t *args)
{
    if (args)
        *args = (args_t){};

    bool mines_set = false;
    args->rows = DEFAULT_ROWS;

    int opt = -1;
    while ((opt = getopt_long(argc, argv, "hm:", long_options, nullptr)) !=
           -1) {
        switch (opt) {
            case 'h':
                printf(USAGE);
                exit(EXIT_SUCCESS);
            case 'm':
                args->mine_cnt = strtoul(optarg, nullptr, 10);
                mines_set = true;
                break;
            default:
                fputs(USAGE, stderr);
                fputs("\n", stderr);
                fputs("Try '" EXE_NAME " -h' for more information.\n", stderr);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
        args->rows = strtoul(argv[optind++], nullptr, 10);

    args->cols = args->rows;
    if (optind < argc)
        args->cols = strtoul(argv[optind++], nullptr, 10);

    uint16_t max_mines = max_mine_cnt(args->rows, args->cols);

    if (!mines_set) {
        uint16_t estimated =
            (uint16_t)(DEFAULT_MINE_DENSITY * args->rows * args->cols);
        args->mine_cnt = MIN(estimated, max_mines);
    }

    if (args->mine_cnt > max_mines) {
        fprintf(stderr, "too many mines for given dimensions (max = %u)\n",
                max_mines);
        exit(EXIT_FAILURE);
    }
}

static tui_t tui = {};

int main(int argc, char *argv[static argc + 1])
{
    srand(time(nullptr));

    args_t args = {};
    parse_args(argc, argv, &args);

    if (!tui_init(&tui, args.rows, args.cols, args.mine_cnt))
        die("tui_init");

    tui_run(&tui);
}
