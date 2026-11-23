#include "extra.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

char *vaprintf(char const fmt[static 1], va_list args)
{
    assert(fmt);

    va_list args_cpy;
    va_copy(args_cpy, args);
    int len = vsnprintf(nullptr, 0, fmt, args_cpy);
    va_end(args_cpy);

    if (len < 0)
        return nullptr;

    char *buf = malloc(len + 1);
    if (buf) {
        if (vsnprintf(buf, len + 1, fmt, args) < 0)
            goto err_cleanup;
    }

    return buf;

err_cleanup:
    free(buf);
    return nullptr;
}

char *aprintf(char const fmt[static 1], ...)
{
    assert(fmt);

    va_list args;
    va_start(args);
    char *retval = vaprintf(fmt, args);
    va_end(args);

    return retval;
}

size_t rand_sz(size_t min, size_t max)
{
    assert(min < max);
    double r = rand() / ((double)RAND_MAX + 1);
    return min + (size_t)(r * (double)(max - min));
}
