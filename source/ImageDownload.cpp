#include "ImageDownload.h"

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <stdexcept>

#pragma comment(lib, "winhttp.lib")

namespace drek_flag_cheat {
    namespace {

        struct UrlParts {
            std::wstring host;
            std::wstring path;
            std::wstring query;
        };

        bool ParseUrl(const std::wstring& url, UrlParts& parts) {
            if (url.empty() || url.size() > 16 * 1024 || url.find_first_of(L"\r\n") != std::wstring::npos) return false;
            URL_COMPONENTS components{};
            components.dwStructSize = sizeof(components);
            components.dwHostNameLength = static_cast<DWORD>(-1);
            components.dwUrlPathLength = static_cast<DWORD>(-1);
            components.dwExtraInfoLength = static_cast<DWORD>(-1);
            if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), ICU_REJECT_USERPWD, &components)) return false;
            if (components.nScheme != INTERNET_SCHEME_HTTPS || components.nPort != INTERNET_DEFAULT_HTTPS_PORT) return false;
            if (!components.lpszHostName || !components.dwHostNameLength) return false;
            parts.host.assign(components.lpszHostName, components.dwHostNameLength);
            std::transform(parts.host.begin(), parts.host.end(), parts.host.begin(),
                [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
            if (parts.host != L"cdn.discordapp.com" && parts.host != L"media.discordapp.net" &&
                parts.host != L"images-ext-1.discordapp.net" && parts.host != L"images-ext-2.discordapp.net") return false;
            parts.path = components.dwUrlPathLength ? std::wstring(components.lpszUrlPath, components.dwUrlPathLength) : L"/";
            parts.query = components.dwExtraInfoLength ? std::wstring(components.lpszExtraInfo, components.dwExtraInfoLength) : L"";
            const auto fragment = parts.query.find(L'#');
            if (fragment != std::wstring::npos) parts.query.resize(fragment);
            return true;
        }

        class HttpHandle {
        public:
            explicit HttpHandle(HINTERNET handle) : handle_(handle) {
                if (!handle_) throw std::runtime_error("Could not create an image download handle.");
            }
            ~HttpHandle() { WinHttpCloseHandle(handle_); }
            HttpHandle(const HttpHandle&) = delete;
            HttpHandle& operator=(const HttpHandle&) = delete;
            HINTERNET Get() const { return handle_; }
        private:
            HINTERNET handle_;
        };

        void RequireHttp(BOOL success, const char* operation) {
            if (!success) {
                throw std::runtime_error(std::string(operation) + " (Windows error " +
                    std::to_string(GetLastError()) + ").");
            }
        }
    } 

    bool IsDiscordImageUrl(const std::wstring& url) {
        UrlParts parts;
        return ParseUrl(url, parts);
    }

    std::vector<std::uint8_t> DownloadDiscordImage(
        const std::wstring& url, const std::function<bool()>& cancelled) {
        UrlParts parts;
        if (!ParseUrl(url, parts)) throw std::runtime_error("Drop an HTTPS Discord image URL.");

        if (parts.host == L"media.discordapp.net" &&
            (parts.path.rfind(L"/attachments/", 0) == 0 || parts.path.rfind(L"/ephemeral-attachments/", 0) == 0)) {
            parts.host = L"cdn.discordapp.com";
        }

        const auto started = std::chrono::steady_clock::now();
        const auto checkCancelled = [&] {
            if (cancelled()) throw std::runtime_error("Image download cancelled.");
            if (std::chrono::steady_clock::now() - started > std::chrono::seconds(20)) {
                throw std::runtime_error("Image download took too long. Try dropping it again.");
            }
            };
        checkCancelled();
        HttpHandle session(WinHttpOpen(L"drek_flag_cheat/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        RequireHttp(WinHttpSetTimeouts(session.Get(), 3000, 3000, 3000, 3000), "Could not set download timeouts");
        HttpHandle connection(WinHttpConnect(session.Get(), parts.host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        const auto resource = parts.path + parts.query;
        HttpHandle request(WinHttpOpenRequest(connection.Get(), L"GET", resource.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION;
        RequireHttp(WinHttpSetOption(request.Get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)),
            "Could not configure the download");
        RequireHttp(WinHttpSendRequest(request.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0), "Could not request the image");
        checkCancelled();
        RequireHttp(WinHttpReceiveResponse(request.Get(), nullptr), "Could not receive the image");
        DWORD status = 0, statusSize = sizeof(status);
        RequireHttp(WinHttpQueryHeaders(request.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX), "Could not read the HTTP status");
        if (status != 200) {
            throw std::runtime_error("Discord returned HTTP " + std::to_string(status) +
                ". The image link may have expired; drop the image again from the current message.");
        }

        std::vector<std::uint8_t> bytes;
        std::array<std::uint8_t, 16 * 1024> chunk{};
        constexpr std::size_t maximumBytes = 32 * 1024 * 1024;
        while (true) {
            checkCancelled();
            DWORD received = 0;
            RequireHttp(WinHttpReadData(request.Get(), chunk.data(), static_cast<DWORD>(chunk.size()), &received),
                "Image download interrupted");
            if (!received) break;
            if (bytes.size() + received > maximumBytes) throw std::runtime_error("Dropped image exceeds 32 MB.");
            bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + received);
        }
        if (bytes.empty()) throw std::runtime_error("Discord returned an empty image.");
        return bytes;
    }

} 
