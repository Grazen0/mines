#pragma once
#include "color.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ESC "\033"
#define CSI ESC "["

typedef enum : uint8_t {
    CLEAR_TO_END = 0,
    CLEAR_TO_BEGIN = 1,
    CLEAR_ALL = 2,
} ansi_clear_t;

typedef enum : uint8_t {
    SGR_RESET = 0,
    SGR_BOLD = 1,
    SGR_ITALIC = 3,
    SGR_FG_BLACK = 30,
    SGR_FG_RED = 31,
    SGR_FG_GREEN = 32,
    SGR_FG_YELLOW = 33,
    SGR_FG_BLUE = 34,
    SGR_FG_MAGENTA = 35,
    SGR_FG_CYAN = 36,
    SGR_FG_WHITE = 37,
    SGR_FG_BRIGHT_BLACK = 90,
    SGR_FG_BRIGHT_RED = 91,
    SGR_FG_BRIGHT_GREEN = 92,
    SGR_FG_BRIGHT_YELLOW = 93,
    SGR_FG_BRIGHT_BLUE = 94,
    SGR_FG_BRIGHT_MAGENTA = 95,
    SGR_FG_BRIGHT_CYAN = 96,
    SGR_FG_BRIGHT_WHITE = 97,
    SGR_BG_BLACK = 40,
    SGR_BG_RED = 41,
    SGR_BG_GREEN = 42,
    SGR_BG_YELLOW = 43,
    SGR_BG_BLUE = 44,
    SGR_BG_MAGENTA = 45,
    SGR_BG_CYAN = 46,
    SGR_BG_WHITE = 47,
    SGR_BG_BRIGHT_BLACK = 100,
    SGR_BG_BRIGHT_RED = 101,
    SGR_BG_BRIGHT_GREEN = 102,
    SGR_BG_BRIGHT_YELLOW = 103,
    SGR_BG_BRIGHT_BLUE = 104,
    SGR_BG_BRIGHT_MAGENTA = 105,
    SGR_BG_BRIGHT_CYAN = 106,
    SGR_BG_BRIGHT_WHITE = 107,
} ansi_sgr_t;

static inline void ansi_clear(ansi_clear_t clear)
{
    printf(CSI "%uJ", clear);
}

static inline void ansi_set_cursor(size_t row, size_t col)
{
    printf(CSI "%zu;%zuH", row, col);
}

static inline void ansi_show_cursor()
{
    printf(CSI "?25h");
}

static inline void ansi_hide_cursor()
{
    printf(CSI "?25l");
}

static inline void ansi_color_reset()
{
    printf(CSI "0m");
}

static inline void ansi_sgr(ansi_sgr_t sgr)
{
    printf(CSI "%um", sgr);
}

static inline void ansi_sgr_fg_rgb(color_t color)
{
    printf(CSI "38;2;%u;%u;%um", color.r, color.g, color.b);
}

static inline void ansi_sgr_bg_rgb(color_t color)
{
    printf(CSI "48;2;%u;%u;%um", color.r, color.g, color.b);
}
