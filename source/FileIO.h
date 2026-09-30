#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace drek_flag_cheat {

std::vector<std::uint8_t> ReadFileBytes(const std::filesystem::path& path,
                                      std::size_t maximumBytes = 32 * 1024 * 1024);

}
