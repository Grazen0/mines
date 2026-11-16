#include "input.h"
#include "ansi.h"
#include <errno.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static struct termios term_old = {};
static bool raw_init = false;

bool enter_raw_mode()
{
    if (raw_init)
        return false;

    tcgetattr(STDIN_FILENO, &term_old);
    struct termios term_new = term_old;
    // - disable software control flow
    // - disable parity checking
    // - strip 8th bit of each input byte
    // - set character size to 8
    term_new.c_iflag &= ~(IXON | INPCK | ISTRIP | CS8);
    // - disable output processing (e.g. Converting "\n" to "\r\n")
    term_new.c_oflag &= ~(OPOST);
    // - enable canonical mode (no buffering)
    // - disable input echoing
    // - disable implementation-defined input processing
    term_new.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
    term_new.c_cc[VMIN] = 0;
    term_new.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_new);

    raw_init = true;
    return true;
}

void exit_raw_mode()
{
    raw_init = false;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_old);
}

char read_key()
{
    char c = 0;

    while (true) {
        ssize_t nread = read(STDIN_FILENO, &c, sizeof(c));
        if (nread != -1)
            break;

        // we also retry on EINTR to resume after a SIGWINCH
        if (errno != EAGAIN && errno != EINTR)
            return -1;
    }

    if (c == ESC[0]) {
        char seq[2] = {};
        if (read(STDIN_FILENO, seq, sizeof(seq)) == -1)
            return c;

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A':
                    return KEY_UP;
                case 'B':
                    return KEY_DOWN;
                case 'C':
                    return KEY_RIGHT;
                case 'D':
                    return KEY_LEFT;
                default:
                    break;
            }
        }
    }

    return c;
}
