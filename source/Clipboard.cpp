#include "Clipboard.h"

#include <cstring>
#include <stdexcept>

namespace drek_flag_cheat {
    namespace {
        class ClipboardSession {
        public:
            explicit ClipboardSession(HWND owner) {
                if (!OpenClipboard(owner)) {
                    throw std::runtime_error("The clipboard is busy. Please try again.");
                }
            }
            ~ClipboardSession() { CloseClipboard(); }
            ClipboardSession(const ClipboardSession&) = delete;
            ClipboardSession& operator=(const ClipboardSession&) = delete;
        };

        class GlobalLockGuard {
        public:
            explicit GlobalLockGuard(HGLOBAL handle) : handle_(handle), pointer_(GlobalLock(handle)) {
                if (!pointer_) throw std::runtime_error("Could not read clipboard memory.");
            }
            ~GlobalLockGuard() { GlobalUnlock(handle_); }
            void* Get() const noexcept { return pointer_; }
        private:
            HGLOBAL handle_;
            void* pointer_;
        };

        Image ReadBitmap(HBITMAP bitmap) {
            BITMAP information{};
            if (!GetObjectW(bitmap, sizeof(information), &information)) {
                throw std::runtime_error("Could not read the clipboard bitmap.");
            }
            ValidateImageSize(information.bmWidth, information.bmHeight);
            Image image;
            image.width = information.bmWidth;
            image.height = information.bmHeight;
            image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);

            BITMAPINFO format{};
            format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            format.bmiHeader.biWidth = image.width;
            format.bmiHeader.biHeight = -image.height;
            format.bmiHeader.biPlanes = 1;
            format.bmiHeader.biBitCount = 32;
            format.bmiHeader.biCompression = BI_RGB;
            HDC screen = GetDC(nullptr);
            if (!screen) throw std::runtime_error("Could not access the bitmap device context.");
            const int rows = GetDIBits(screen, bitmap, 0, static_cast<UINT>(image.height), image.pixels.data(),
                &format, DIB_RGB_COLORS);
            ReleaseDC(nullptr, screen);
            if (rows != image.height) {
                throw std::runtime_error("Could not copy all clipboard bitmap rows.");
            }
            for (auto& pixel : image.pixels) pixel.alpha = 255;
            return image;
        }
    }

    ClipboardImage ReadClipboardImage(HWND owner) {
        ClipboardSession session(owner);
        ClipboardImage result;
        const UINT pngFormat = RegisterClipboardFormatW(L"PNG");
        if (pngFormat && IsClipboardFormatAvailable(pngFormat)) {
            const auto handle = static_cast<HGLOBAL>(GetClipboardData(pngFormat));
            if (handle) {
                const SIZE_T size = GlobalSize(handle);
                if (size > 0 && size <= 32 * 1024 * 1024) {
                    GlobalLockGuard lock(handle);
                    const auto* begin = static_cast<const std::uint8_t*>(lock.Get());
                    result.encodedPng.assign(begin, begin + size);
                }
            }
        }
        if (IsClipboardFormatAvailable(CF_BITMAP)) {
            const auto bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
            if (bitmap) {
                try {
                    result.bitmap = ReadBitmap(bitmap);
                }
                catch (...) {
                    if (result.encodedPng.empty()) throw;
                }
            }
        }
        if (result.encodedPng.empty() && !result.bitmap) {
            throw std::runtime_error("Copy the flag image itself, then paste here. A copied link is not an image.");
        }
        return result;
    }

    void CopyText(HWND owner, const std::wstring& text) {
        if (text.empty()) return;
        const SIZE_T size = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory) throw std::runtime_error("Could not allocate clipboard text.");
        try {
            {
                GlobalLockGuard lock(memory);
                std::memcpy(lock.Get(), text.c_str(), size);
            }
            ClipboardSession session(owner);
            if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory)) {
                throw std::runtime_error("Could not copy the country name.");
            }
            memory = nullptr;
        }
        catch (...) {
            if (memory) GlobalFree(memory);
            throw;
        }
    }

}
