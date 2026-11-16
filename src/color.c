#include "color.h"
#include "extra.h"

color_t color_brighten(color_t color, uint8_t add)
{
    return (color_t){
        MIN(255, color.r + add),
        MIN(255, color.g + add),
        MIN(255, color.b + add),
    };
}
