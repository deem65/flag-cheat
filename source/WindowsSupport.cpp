#include "WindowsSupport.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <combaseapi.h>

namespace drek_flag_cheat {

    std::wstring Utf8ToWide(const std::string& text) {
        if (text.empty()) return {};
        const int required = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!required) return L"Unable to display message.";
        std::wstring result(required, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required);
        return result;
    }

    std::wstring LowercaseInvariant(const std::wstring& text) {
        if (text.empty()) return {};
        const int required = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr, 0);
        if (!required) throw std::runtime_error("Could not lowercase the country name.");
        std::wstring result(required, L'\0');
        if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(),
            static_cast<int>(text.size()), result.data(), required, nullptr, nullptr, 0)) {
            throw std::runtime_error("Could not lowercase the country name.");
        }
        return result;
    }

    std::filesystem::path ExecutableDirectory() {
        std::vector<wchar_t> path(32768);
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size()) {
            throw std::runtime_error("Could not find the application directory.");
        }
        return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
    }

    void CheckHResult(HRESULT result, const char* operation) {
        if (FAILED(result)) {
            std::ostringstream message;
            message << operation << " failed (0x" << std::hex << static_cast<unsigned long>(result) << ").";
            throw std::runtime_error(message.str());
        }
    }

    ComApartment::ComApartment() {
        CheckHResult(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "COM initialization");
    }

    ComApartment::~ComApartment() {
        CoUninitialize();
    }

} 
