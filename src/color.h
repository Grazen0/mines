#pragma once
#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

[[nodiscard]] color_t color_brighten(color_t color, uint8_t add);

#define UNFOLD(color) (color).r, (color).g, (color).b
