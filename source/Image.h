#pragma once

#include <cstdint>
#include <vector>

namespace drek_flag_cheat {

struct Pixel {
    std::uint8_t blue = 0;
    std::uint8_t green = 0;
    std::uint8_t red = 0;
    std::uint8_t alpha = 255;
};
static_assert(sizeof(Pixel) == 4);

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Pixel> pixels;
};

void ValidateImageSize(int width, int height);
void ValidateImage(const Image& image);
void CompositeOnWhite(Image& image);


std::vector<float> ResizeRgb(const Image& image, int width, int height);

}
