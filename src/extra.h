#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Allocate and print formatted string.
 *
 * Poor man's implementation of GNU's vaprintf.
 *
 * @return A pointer to the first character of the formatted string, or NULL if
 * an error ocurred.
 */
[[nodiscard]] char *vaprintf(char const fmt[static 1], va_list args);

/**
 * @brief Allocate and print formatted string.
 *
 * Poor man's implementation of GNU's aprintf.
 *
 * @return A pointer to the first character of the formatted string, or NULL if
 * an error ocurred.
 */
[[nodiscard, gnu::format(printf, 1, 2)]] char *aprintf(char const fmt[static 1],
                                                       ...);

/**
 * @brief Terminates the program with a message.
 *
 * Prints s through `perror`.
 *
 * @sa perror
 */
[[noreturn]] static inline void die(char const s[static 1])
{
    perror(s);
    exit(EXIT_FAILURE);
}
