#include "Matcher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace drek_flag_cheat {
    namespace {
        constexpr int coarseWidth = 32;
        constexpr int coarseHeight = 20;
        constexpr int fineWidth = 160;
        constexpr int fineHeight = 96;
        constexpr int tileColumns = 8;
        constexpr int tileRows = 6;

        double MeanAbsoluteError(const std::vector<float>& first, const std::vector<float>& second) {
            double sum = 0;
            for (std::size_t index = 0; index < first.size(); ++index) {
                sum += std::abs(first[index] - second[index]);
            }
            return sum / static_cast<double>(first.size());
        }

        double AspectError(double first, double second) {
            return std::min(1.0, std::abs(std::log(first / second)));
        }

        std::vector<float> MakeEdges(const std::vector<float>& rgb) {
            std::vector<float> luminance(fineWidth * fineHeight);
            for (std::size_t pixel = 0; pixel < luminance.size(); ++pixel) {
                luminance[pixel] = 0.2126f * rgb[pixel * 3] + 0.7152f * rgb[pixel * 3 + 1] +
                    0.0722f * rgb[pixel * 3 + 2];
            }
            std::vector<float> edges(luminance.size() * 2, 0.0f);
            for (int y = 0; y < fineHeight; ++y) {
                for (int x = 0; x < fineWidth; ++x) {
                    const int pixel = y * fineWidth + x;
                    if (x + 1 < fineWidth) {
                        edges[pixel * 2] = luminance[pixel + 1] - luminance[pixel];
                    }
                    if (y + 1 < fineHeight) {
                        edges[pixel * 2 + 1] = luminance[pixel + fineWidth] - luminance[pixel];
                    }
                }
            }
            return edges;
        }

        double DetailError(const std::vector<float>& first, const std::vector<float>& second) {
            std::array<double, tileColumns* tileRows> errors{};
            constexpr int tileWidth = fineWidth / tileColumns;
            constexpr int tileHeight = fineHeight / tileRows;
            for (int y = 0; y < fineHeight; ++y) {
                for (int x = 0; x < fineWidth; ++x) {
                    const int tile = (y / tileHeight) * tileColumns + x / tileWidth;
                    const int component = (y * fineWidth + x) * 3;
                    for (int channel = 0; channel < 3; ++channel) {
                        errors[tile] += std::abs(first[component + channel] - second[component + channel]);
                    }
                }
            }
            std::sort(errors.begin(), errors.end(), std::greater<double>());
            constexpr int importantTiles = 4;
            const double sum = std::accumulate(errors.begin(), errors.begin() + importantTiles, 0.0);
            return sum / (importantTiles * tileWidth * tileHeight * 3);
        }
    }

    ImageFeatures DescribeImage(const Image& image) {
        ValidateImage(image);
        ImageFeatures features;
        features.sourceWidth = image.width;
        features.sourceHeight = image.height;
        features.aspectRatio = static_cast<double>(image.width) / image.height;
        features.coarseRgb = ResizeRgb(image, coarseWidth, coarseHeight);
        features.fineRgb = ResizeRgb(image, fineWidth, fineHeight);
        double variance = 0;
        constexpr int pixelCount = fineWidth * fineHeight;
        for (int channel = 0; channel < 3; ++channel) {
            double sum = 0, squaredSum = 0;
            for (int pixel = 0; pixel < pixelCount; ++pixel) {
                const double value = features.fineRgb[pixel * 3 + channel];
                sum += value;
                squaredSum += value * value;
            }
            const double mean = sum / pixelCount;
            variance += std::max(0.0, squaredSum / pixelCount - mean * mean);
        }
        features.colourVariation = std::sqrt(variance / 3.0);
        features.edges = MakeEdges(features.fineRgb);
        return features;
    }

    void Matcher::AddReference(FlagRecord record, const Image& image) {
        references_.push_back({ std::move(record), DescribeImage(image) });
    }

    std::size_t Matcher::ReferenceCount() const noexcept {
        return references_.size();
    }

    std::vector<Comparison> Matcher::Compare(const ImageFeatures& query, int shortlistSize) const {
        if (references_.empty()) {
            throw std::runtime_error("No reference flags are loaded.");
        }
        if (query.coarseRgb.size() != coarseWidth * coarseHeight * 3 ||
            query.fineRgb.size() != fineWidth * fineHeight * 3 ||
            query.edges.size() != fineWidth * fineHeight * 2 || !std::isfinite(query.aspectRatio) || query.aspectRatio <= 0) {
            throw std::runtime_error("Invalid image features.");
        }
        const auto count = std::min(references_.size(), static_cast<std::size_t>(std::max(1, shortlistSize)));
        std::vector<std::pair<double, std::size_t>> ranked;
        ranked.reserve(references_.size());
        for (std::size_t index = 0; index < references_.size(); ++index) {
            const auto& reference = references_[index].features;
            const double error = 0.96 * MeanAbsoluteError(query.coarseRgb, reference.coarseRgb) +
                0.04 * AspectError(query.aspectRatio, reference.aspectRatio);
            ranked.emplace_back(error, index);
        }
        std::partial_sort(ranked.begin(), ranked.begin() + count, ranked.end());
        std::vector<Comparison> comparisons;
        comparisons.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto& reference = references_[ranked[index].second];
            comparisons.push_back({
                reference.record.code,
                reference.record.name,
                MeanAbsoluteError(query.fineRgb, reference.features.fineRgb),
                DetailError(query.fineRgb, reference.features.fineRgb),
                MeanAbsoluteError(query.edges, reference.features.edges) / 2.0,
                AspectError(query.aspectRatio, reference.features.aspectRatio)
                });
        }
        return comparisons;
    }

}
