#pragma once

#include "Image.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace drek_flag_cheat {

class ImageDecoder {
public:
    ImageDecoder();
    Image Decode(const std::vector<std::uint8_t>& bytes) const;
    Image Read(const std::filesystem::path& path) const;

private:
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory_;
};

} 
