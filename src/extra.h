#pragma once
#include <stdarg.h>

[[nodiscard]] char *vaprintf(char const fmt[static 1], va_list args);

[[nodiscard, gnu::format(printf, 1, 2)]] char *aprintf(char const fmt[static 1],
                                                       ...);
