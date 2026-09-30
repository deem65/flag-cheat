#include "FileIO.h"
#include "Image.h"
#include "LuaPolicy.h"
#include "Matcher.h"

#ifdef _WIN32
#include "ImageDecoder.h"
#include "WindowsSupport.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using namespace drek_flag_cheat;

    void Require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    class TestImages {
    public:
        explicit TestImages(std::filesystem::path root) : root_(std::move(root)) {}

        Image Read(const std::filesystem::path& relativePath) const {
#ifdef _WIN32
            return decoder_.Read(root_ / relativePath);
#else
            // Portable tests receive the same decoded pixels, prepared by Pillow.
            // The Windows test project exercises the real WIC decoder instead.
            auto path = root_ / relativePath;
            path.replace_extension(".bgra");
            const auto bytes = ReadFileBytes(path, 64 * 1024 * 1024 + 8);
            Require(bytes.size() >= 8, "Truncated portable test image.");
            auto read32 = [&bytes](std::size_t at) {
                return static_cast<unsigned>(bytes[at]) | (static_cast<unsigned>(bytes[at + 1]) << 8) |
                    (static_cast<unsigned>(bytes[at + 2]) << 16) | (static_cast<unsigned>(bytes[at + 3]) << 24);
                };
            Image image;
            image.width = static_cast<int>(read32(0));
            image.height = static_cast<int>(read32(4));
            ValidateImageSize(image.width, image.height);
            image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);
            Require(bytes.size() == 8 + image.pixels.size() * 4, "Invalid portable test image size.");
            std::memcpy(image.pixels.data(), bytes.data() + 8, bytes.size() - 8);
            return image;
#endif
        }

    private:
        std::filesystem::path root_;
#ifdef _WIN32
        ImageDecoder decoder_;
#endif
    };

    Image ResizeForTest(const Image& image, int width) {
        Image smaller;
        smaller.width = width;
        smaller.height = std::max(1, static_cast<int>(std::lround(image.height * static_cast<double>(width) / image.width)));
        const auto rgb = ResizeRgb(image, smaller.width, smaller.height);
        smaller.pixels.resize(static_cast<std::size_t>(smaller.width) * smaller.height);
        const auto byte = [](float value) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(value * 255.0f), 0L, 255L));
            };
        for (std::size_t pixel = 0; pixel < smaller.pixels.size(); ++pixel) {
            smaller.pixels[pixel] = { byte(rgb[pixel * 3 + 2]), byte(rgb[pixel * 3 + 1]), byte(rgb[pixel * 3]), 255 };
        }
        return smaller;
    }

    bool Contains(const Recognition& result, const std::string& code) {
        return std::any_of(result.candidates.begin(), result.candidates.end(), [&code](const Candidate& candidate) {
            return candidate.code == code;
            });
    }

    int RunTests(const std::filesystem::path& projectRoot, const std::filesystem::path& imageRoot) {
#ifdef _WIN32
        ComApartment apartment;
#endif
        TestImages images(imageRoot);
        Matcher matcher;
        LuaPolicy policy(matcher);
        const auto catalog = policy.LoadCatalog(projectRoot / "assets" / "flags.lua");
        for (const auto& record : catalog) {
            matcher.AddReference(record, images.Read(std::filesystem::path("assets/flags") / record.file));
        }
        policy.LoadPolicy(projectRoot / "lua" / "recognize.lua");
        Require(matcher.ReferenceCount() == catalog.size(), "Reference collection did not load completely.");

        for (const auto& example : std::vector<std::pair<std::string, std::string>>{
            {"tonga.png", "to"}, {"albania.png", "al"},
            {"tonga-jpeg.png", "to"}, {"albania-jpeg.png", "al"} }) {
            const auto input = images.Read(std::filesystem::path("tests/fixtures") / example.first);
            const auto begin = std::chrono::steady_clock::now();
            const auto result = policy.Recognize(DescribeImage(input));
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            Require(result.status == MatchStatus::Match && result.candidates.front().code == example.second,
                "Supplied example failed: " + example.first);
            std::cout << "PASS " << example.first << " -> " << result.candidates.front().name
                << " (" << elapsed << " ms, error " << result.candidates.front().error << ")\n";
        }

        std::size_t originalTop = 0, originalIncluded = 0, originalAmbiguous = 0;
        std::size_t smallTop = 0, smallIncluded = 0, smallAmbiguous = 0;
        std::vector<double> timings;
        for (const auto& record : catalog) {
            const auto input = images.Read(std::filesystem::path("assets/flags") / record.file);
            const auto original = policy.Recognize(DescribeImage(input));
            originalTop += original.candidates.front().code == record.code;
            originalIncluded += Contains(original, record.code);
            originalAmbiguous += original.status == MatchStatus::Ambiguous;
            Require(Contains(original, record.code), "Original reference omitted its country: " + record.code);

            const auto smaller = ResizeForTest(input, 80);
            const auto begin = std::chrono::steady_clock::now();
            const auto result = policy.Recognize(DescribeImage(smaller));
            timings.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
            smallTop += result.candidates.front().code == record.code;
            smallIncluded += Contains(result, record.code);
            smallAmbiguous += result.status == MatchStatus::Ambiguous;
            if (!Contains(result, record.code)) {
                std::cout << "MISS at 80px: " << record.code << " -> " << result.candidates.front().code << '\n';
            }
        }
        std::sort(timings.begin(), timings.end());
        std::cout << "Original references: top=" << originalTop << '/' << catalog.size()
            << ", included=" << originalIncluded << '/' << catalog.size()
            << ", ambiguous=" << originalAmbiguous << '\n';
        std::cout << "80px resized references: top=" << smallTop << '/' << catalog.size()
            << ", included=" << smallIncluded << '/' << catalog.size()
            << ", ambiguous=" << smallAmbiguous << '\n';
        std::cout << "80px matching time: median=" << timings[timings.size() / 2]
            << " ms, p95=" << timings[static_cast<std::size_t>((timings.size() - 1) * 0.95)] << " ms\n";

        const auto norway = policy.Recognize(DescribeImage(images.Read("assets/flags/no.png")));
        Require(norway.status == MatchStatus::Ambiguous && Contains(norway, "no") && Contains(norway, "bv"),
            "Identical Norwegian/Bouvet artwork must remain ambiguous.");
        std::cout << "PASS duplicate-artwork ambiguity\n";

        for (const Pixel colour : {Pixel{ 255, 255, 255, 255 }, Pixel{ 0, 0, 255, 255 }, Pixel{ 0, 0, 0, 255 }}) {
            Image blank{ 80, 50, std::vector<Pixel>(80 * 50, colour) };
            Require(policy.Recognize(DescribeImage(blank)).status == MatchStatus::Uncertain,
                "A solid-colour image must not receive a confident answer.");
        }
        std::cout << "PASS solid-colour rejection\n";

        Image transparent{ 1, 1, {Pixel{0, 0, 0, 0}} };
        CompositeOnWhite(transparent);
        Require(transparent.pixels.front().red == 255 && transparent.pixels.front().alpha == 255,
            "Transparent pixels must be composited on white.");
        bool rejected = false;
        try { ValidateImageSize(100000, 100000); }
        catch (const std::exception&) { rejected = true; }
        Require(rejected, "Oversized decoded buffers must be rejected.");
        std::cout << "PASS alpha handling and image size bounds\n";

        Require(smallIncluded == catalog.size(), "Some resized flags fell outside the returned suggestions.");
        std::cout << "All core checks passed. Windows clipboard/UI behavior requires Windows testing.\n";
        return 0;
    }
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    const auto root = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path();
    try { return RunTests(root, root); }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
#else
int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "Usage: core-tests PROJECT_ROOT DECODED_IMAGE_ROOT\n"; return 2; }
    try { return RunTests(argv[1], argv[2]); }
    catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
#endif
