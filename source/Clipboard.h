#pragma once

#include "Image.h"

#include <Windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace drek_flag_cheat {

    struct ClipboardImage {
        std::vector<std::uint8_t> encodedPng;
        std::optional<Image> bitmap;
        std::wstring imageUrl; // nonempty when a drop supplies a URL instead of pixels
    };

    ClipboardImage ReadClipboardImage(HWND owner);
    void CopyText(HWND owner, const std::wstring& text);

}
