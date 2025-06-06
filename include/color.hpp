#pragma once

#include <cstdint>

struct Color
{
    uint8_t red  { 0 };
    uint8_t green{ 0 };
    uint8_t blue { 0 };
    uint8_t alpha{ 0 };

    Color() = default;

    Color(uint32_t rgba)
        : red  { (rgba << 24) & 0xFF }
        , green{ (rgba << 16) & 0xFF }
        , blue { (rgba <<  8) & 0xFF }
        , alpha{ (rgba <<  0) & 0xFF }
    { }

    Color (uint32_t rgb, uint8_t alpha_) // hex representation of rgb, decimal for alpha
        : red  { (rgb << 24) & 0xFF }
        , green{ (rgb << 16) & 0xFF }
        , blue { (rgb <<  8) & 0xFF }
        , alpha{ alpha_ }
    { }

    Color(uint8_t red_, uint8_t green_, uint8_t blue_, uint8_t alpha_ = 0)
        : red  { red_   }
        , green{ green_ }
        , blue { blue_  }
        , alpha{ alpha_ }
    { }

    uint32_t toRGBA()
    {
        return (red <<   24) |
               (green << 16) |
               (blue <<   8) |
               (alpha <<  0);
    }
};
