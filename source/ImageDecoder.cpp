#include "ImageDecoder.h"
#include "FileIO.h"
#include "WindowsSupport.h"

#include <limits>
#include <stdexcept>

namespace drek_flag_cheat {
    using Microsoft::WRL::ComPtr;

    ImageDecoder::ImageDecoder() {
        CheckHResult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(factory_.GetAddressOf())), "Creating the Windows image decoder");
    }

    Image ImageDecoder::Decode(const std::vector<std::uint8_t>& bytes) const {
        if (bytes.empty() || bytes.size() > 32 * 1024 * 1024) {
            throw std::runtime_error("Encoded image is empty or larger than 32 MB.");
        }
        ComPtr<IWICStream> stream;
        CheckHResult(factory_->CreateStream(stream.GetAddressOf()), "Creating an image stream");
        CheckHResult(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),
            static_cast<DWORD>(bytes.size())), "Reading encoded image data");
        ComPtr<IWICBitmapDecoder> decoder;
        CheckHResult(factory_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf()), "Decoding the clipboard image");
        ComPtr<IWICBitmapFrameDecode> frame;
        CheckHResult(decoder->GetFrame(0, frame.GetAddressOf()), "Reading the image frame");
        UINT width = 0, height = 0;
        CheckHResult(frame->GetSize(&width, &height), "Reading image dimensions");
        if (width > static_cast<UINT>(std::numeric_limits<int>::max()) ||
            height > static_cast<UINT>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("Image dimensions exceed supported limits.");
        }
        ValidateImageSize(static_cast<int>(width), static_cast<int>(height));

        ComPtr<IWICFormatConverter> converter;
        CheckHResult(factory_->CreateFormatConverter(converter.GetAddressOf()), "Creating a pixel converter");
        CheckHResult(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom), "Converting image pixels");
        Image image;
        image.width = static_cast<int>(width);
        image.height = static_cast<int>(height);
        image.pixels.resize(static_cast<std::size_t>(width) * height);
        CheckHResult(converter->CopyPixels(nullptr, width * sizeof(Pixel),
            static_cast<UINT>(image.pixels.size() * sizeof(Pixel)),
            reinterpret_cast<BYTE*>(image.pixels.data())), "Copying image pixels");
        CompositeOnWhite(image);
        return image;
    }

    Image ImageDecoder::Read(const std::filesystem::path& path) const {
        return Decode(ReadFileBytes(path));
    }

} 
