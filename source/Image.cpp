#include "Image.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace drek_flag_cheat {

void ValidateImageSize(int width, int height) {
    constexpr std::int64_t maximumPixels = 16'000'000;
    constexpr int maximumSide = 16'384;
    if (width <= 0 || height <= 0 || width > maximumSide || height > maximumSide ||
        static_cast<std::int64_t>(width) * height > maximumPixels) {
        throw std::runtime_error("Image is empty or too large (maximum 16 million pixels).");
    }
}

void ValidateImage(const Image& image) {
    ValidateImageSize(image.width, image.height);
    if (image.pixels.size() != static_cast<std::size_t>(image.width) * image.height) {
        throw std::runtime_error("Image buffer does not match its dimensions.");
    }
}

void CompositeOnWhite(Image& image) {
    for (auto& pixel : image.pixels) {
        const unsigned alpha = pixel.alpha;
        const auto blend = [alpha](unsigned channel) {
            return static_cast<std::uint8_t>((channel * alpha + 255 * (255 - alpha) + 127) / 255);
        };
        pixel.red = blend(pixel.red);
        pixel.green = blend(pixel.green);
        pixel.blue = blend(pixel.blue);
        pixel.alpha = 255;
    }
}

namespace {
struct SampleWeight {
    int source;
    float weight;
};

using SampleTable = std::vector<std::vector<SampleWeight>>;

SampleTable MakeSampleTable(int sourceSize, int targetSize) {
    SampleTable table(targetSize);
    const double scale = static_cast<double>(sourceSize) / targetSize;
    for (int target = 0; target < targetSize; ++target) {
        const double begin = target * scale;
        const double end = (target + 1) * scale;
        for (int source = static_cast<int>(std::floor(begin));
             source < std::min(sourceSize, static_cast<int>(std::ceil(end))); ++source) {
            const double overlap = std::min(end, source + 1.0) - std::max(begin, static_cast<double>(source));
            if (overlap > 0) {
                table[target].push_back({source, static_cast<float>(overlap / scale)});
            }
        }
    }
    return table;
}
} 

std::vector<float> ResizeRgb(const Image& image, int width, int height) {
    ValidateImage(image);
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        throw std::runtime_error("Invalid comparison grid size.");
    }
    const auto horizontalWeights = MakeSampleTable(image.width, width);
    const auto verticalWeights = MakeSampleTable(image.height, height);
    const std::size_t rowSize = static_cast<std::size_t>(width) * 3;
    std::vector<float> horizontal(rowSize * image.height, 0.0f);
    std::vector<float> result(rowSize * height, 0.0f);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto* target = &horizontal[static_cast<std::size_t>(y) * rowSize + x * 3];
            for (const auto sample : horizontalWeights[x]) {
                const auto& pixel = image.pixels[static_cast<std::size_t>(y) * image.width + sample.source];
                const float weight = sample.weight / 255.0f;
                target[0] += pixel.red * weight;
                target[1] += pixel.green * weight;
                target[2] += pixel.blue * weight;
            }
        }
    }
    for (int y = 0; y < height; ++y) {
        auto* target = &result[static_cast<std::size_t>(y) * rowSize];
        for (const auto sample : verticalWeights[y]) {
            const auto* source = &horizontal[static_cast<std::size_t>(sample.source) * rowSize];
            for (std::size_t component = 0; component < rowSize; ++component) {
                target[component] += source[component] * sample.weight;
            }
        }
    }
    return result;
}

} 
