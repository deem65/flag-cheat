#pragma once

#include "Image.h"

#include <string>
#include <vector>

namespace drek_flag_cheat {

    struct FlagRecord {
        std::string code;
        std::string name;
        std::string file;
    };

    struct ImageFeatures {
        int sourceWidth = 0;
        int sourceHeight = 0;
        double aspectRatio = 1.0;
        double colourVariation = 0;
        std::vector<float> coarseRgb;
        std::vector<float> fineRgb;
        std::vector<float> edges;
    };

    struct Comparison {
        std::string code;
        std::string name;
        double colourError = 0;
        double detailError = 0;
        double edgeError = 0;
        double aspectError = 0;
    };

    ImageFeatures DescribeImage(const Image& image);
    class Matcher {
    public:
        void AddReference(FlagRecord record, const Image& image);
        [[nodiscard]] std::size_t ReferenceCount() const noexcept;
        [[nodiscard]] std::vector<Comparison> Compare(const ImageFeatures& query, int shortlistSize) const;

    private:
        struct Reference {
            FlagRecord record;
            ImageFeatures features;
        };
        std::vector<Reference> references_;
    };

}
