#include "color.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

color_t color_brighten(color_t color, uint8_t add)
{
    return (color_t){
        MIN(255, color.r + add),
        MIN(255, color.g + add),
        MIN(255, color.b + add),
    };
}
