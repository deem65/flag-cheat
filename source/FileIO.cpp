#include "FileIO.h"

#include <fstream>
#include <stdexcept>

namespace drek_flag_cheat {

    std::vector<std::uint8_t> ReadFileBytes(const std::filesystem::path& path, std::size_t maximumBytes) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + path.u8string());
        }
        const auto length = file.tellg();
        if (length <= 0 || static_cast<std::uint64_t>(length) > maximumBytes) {
            throw std::runtime_error("File is empty or too large: " + path.u8string());
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
            throw std::runtime_error("Could not read file: " + path.u8string());
        }
        return bytes;
    }

} 
