#pragma once

#define CTRL_KEY(key) ((key) & 0x1F)
#define SHIFT_KEY(key) ((key) & 0x1F)

typedef enum : char {
    KEY_EXIT = CTRL_KEY('c'),
    KEY_DOWN = 'j',
    KEY_RIGHT = 'l',
    KEY_UP = 'k',
    KEY_LEFT = 'h',
    KEY_DOWN_FAST = 'J',
    KEY_RIGHT_FAST = 'L',
    KEY_UP_FAST = 'K',
    KEY_LEFT_FAST = 'H',
    KEY_FLAG = 'f',
    KEY_FLAG_ALT = 'F',
    KEY_REVEAL = 32, // space
    KEY_REVEAL_ALT = 10, // return
    KEY_RESET = CTRL_KEY('r'),
} key_t;

// returns -1 on error
char read_key();

bool enter_raw_mode();

void exit_raw_mode();
