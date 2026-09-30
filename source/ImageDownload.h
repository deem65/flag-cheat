#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace drek_flag_cheat {

bool IsDiscordImageUrl(const std::wstring& url);

std::vector<std::uint8_t> DownloadDiscordImage(
    const std::wstring& url, const std::function<bool()>& cancelled);

}
