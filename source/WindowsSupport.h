#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>

namespace drek_flag_cheat {

std::wstring Utf8ToWide(const std::string& text);
std::wstring LowercaseInvariant(const std::wstring& text);
std::filesystem::path ExecutableDirectory();
void CheckHResult(HRESULT result, const char* operation);

class ComApartment {
public:
    ComApartment();
    ~ComApartment();
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
};

}
