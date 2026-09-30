#include "DiscordDrop.h"
#include "WindowsSupport.h"
#include "ImageDownload.h"

#include <ole2.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace drek_flag_cheat {
    namespace {

        struct MediumGuard {
            STGMEDIUM value{};
            ~MediumGuard() {
                if (value.tymed != TYMED_NULL) ReleaseStgMedium(&value);
            }
        };

        struct MemoryLock {
            HGLOBAL handle;
            void* pointer;
            explicit MemoryLock(HGLOBAL memory) : handle(memory), pointer(GlobalLock(memory)) {}
            ~MemoryLock() { if (pointer) GlobalUnlock(handle); }
        };

        FORMATETC Format(CLIPFORMAT format) {
            return { format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        }

        std::vector<std::uint8_t> ReadBytes(IDataObject* object, CLIPFORMAT format, std::size_t limit) {
            if (!format) return {};
            auto request = Format(format);
            MediumGuard medium;
            if (FAILED(object->GetData(&request, &medium.value))) return {};
            if (medium.value.tymed != TYMED_HGLOBAL || !medium.value.hGlobal) return {};
            const auto size = GlobalSize(medium.value.hGlobal);
            if (!size || size > limit) return {};
            MemoryLock lock(medium.value.hGlobal);
            if (!lock.pointer) return {};
            const auto* begin = static_cast<const std::uint8_t*>(lock.pointer);
            return { begin, begin + size };
        }

        std::wstring ReadText(IDataObject* object, CLIPFORMAT format, bool unicode) {
            const auto bytes = ReadBytes(object, format, 256 * 1024);
            if (bytes.empty()) return {};
            if (unicode) {
                std::wstring text(bytes.size() / sizeof(wchar_t), L'\0');
                std::memcpy(text.data(), bytes.data(), text.size() * sizeof(wchar_t));
                const auto end = text.find(L'\0');
                if (end != std::wstring::npos) text.resize(end);
                return text;
            }
            const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{ 0 });
            return Utf8ToWide(std::string(bytes.begin(), end));
        }

        void ReplaceAll(std::wstring& text, const std::wstring& from, const std::wstring& to) {
            std::size_t at = 0;
            while ((at = text.find(from, at)) != std::wstring::npos) {
                text.replace(at, from.size(), to);
                at += to.size();
            }
        }

        std::wstring FindImageUrl(const std::wstring& text) {
            std::size_t at = 0;
            while ((at = text.find(L"https://", at)) != std::wstring::npos) {
                const auto end = text.find_first_of(L" \r\n\t\"'<>", at);
                auto url = text.substr(at, end == std::wstring::npos ? end : end - at);
                ReplaceAll(url, L"&amp;", L"&");
                ReplaceAll(url, L"&#38;", L"&");
                ReplaceAll(url, L"&#x26;", L"&");
                if (IsDiscordImageUrl(url)) return url;
                at += 8;
            }
            return {};
        }

        class DiscordDropTarget final : public IDropTarget {
        public:
            DiscordDropTarget(std::function<void(ClipboardImage)> onDrop,
                std::function<void(const std::string&)> onError)
                : onDrop_(std::move(onDrop)), onError_(std::move(onError)) {
                png_ = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"PNG"));
                unicodeUrl_ = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"UniformResourceLocatorW"));
                ansiUrl_ = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"UniformResourceLocator"));
                html_ = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"HTML Format"));
            }

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** output) override {
                if (!output) return E_POINTER;
                *output = nullptr;
                if (id != IID_IUnknown && id != IID_IDropTarget) return E_NOINTERFACE;
                *output = static_cast<IDropTarget*>(this);
                AddRef();
                return S_OK;
            }

            ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }

            ULONG STDMETHODCALLTYPE Release() override {
                const ULONG remaining = --references_;
                if (!remaining) delete this;
                return remaining;
            }

            HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* object, DWORD, POINTL, DWORD* effect) override {
                if (!effect) return E_POINTER;
                accepts_ = false;
                if (object) {
                    const CLIPFORMAT formats[] = { png_, unicodeUrl_, ansiUrl_, CF_UNICODETEXT, CF_TEXT, html_ };
                    for (const auto format : formats) {
                        if (!format) continue;
                        auto request = Format(format);
                        if (object->QueryGetData(&request) == S_OK) {
                            accepts_ = true;
                            break;
                        }
                    }
                }
                *effect = accepts_ ? AllowedEffect(*effect) : DROPEFFECT_NONE;
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) override {
                if (!effect) return E_POINTER;
                *effect = accepts_ ? AllowedEffect(*effect) : DROPEFFECT_NONE;
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE DragLeave() override {
                accepts_ = false;
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE Drop(IDataObject* object, DWORD, POINTL, DWORD* effect) override {
                if (!effect) return E_POINTER;
                const DWORD allowed = AllowedEffect(*effect);
                *effect = DROPEFFECT_NONE;
                accepts_ = false;
                if (!object || !allowed) return S_OK;
                try {
                    ClipboardImage image;
                    image.encodedPng = ReadBytes(object, png_, 32 * 1024 * 1024);
                    if (image.encodedPng.empty()) {
                        const std::pair<CLIPFORMAT, bool> formats[] = {
                            {unicodeUrl_, true}, {ansiUrl_, false}, {CF_UNICODETEXT, true},
                            {CF_TEXT, false}, {html_, false}
                        };
                        for (const auto& format : formats) {
                            image.imageUrl = FindImageUrl(ReadText(object, format.first, format.second));
                            if (!image.imageUrl.empty()) break;
                        }
                        if (image.imageUrl.empty()) {
                            throw std::runtime_error(
                                "This drop contains no supported Discord image URL or PNG. Try Copy Image and Ctrl+V.");
                        }
                    }
                    onDrop_(std::move(image));
                    *effect = allowed;
                }
                catch (const std::exception& error) {
                    ReportError(error.what());
                }
                catch (...) {
                    ReportError("Could not read the dropped image.");
                }
                return S_OK;
            }

        private:
            static DWORD AllowedEffect(DWORD offered) {
                if (offered & DROPEFFECT_COPY) return DROPEFFECT_COPY;
                if (offered & DROPEFFECT_LINK) return DROPEFFECT_LINK;
                return DROPEFFECT_NONE;
            }

            void ReportError(const char* message) noexcept {
                try { onError_(message); }
                catch (...) { /* never unwind across the COM boundary */ }
            }

            std::atomic<ULONG> references_{ 1 };
            bool accepts_ = false;
            CLIPFORMAT png_ = 0, unicodeUrl_ = 0, ansiUrl_ = 0, html_ = 0;
            std::function<void(ClipboardImage)> onDrop_;
            std::function<void(const std::string&)> onError_;
        };
    } 

    void RegisterDiscordDrop(HWND window, std::function<void(ClipboardImage)> onDrop,
        std::function<void(const std::string&)> onError) {
        auto* target = new DiscordDropTarget(std::move(onDrop), std::move(onError));
        const HRESULT result = RegisterDragDrop(window, target);
        target->Release();
        CheckHResult(result, "Registering image drag-and-drop");
    }

}
