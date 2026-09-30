#pragma once

#include "Clipboard.h"
#include "LuaPolicy.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace drek_flag_cheat {

constexpr UINT recognitionEventMessage = WM_APP + 1;

struct WorkerEvent {
    enum class Kind { Ready, Result, Error } kind = Kind::Error;
    std::uint64_t requestId = 0;
    std::size_t referenceCount = 0;
    double elapsedMilliseconds = 0;
    std::string error;
    Image preview;
    Recognition recognition;
};

class RecognitionWorker {
public:
    ~RecognitionWorker();
    void Start(HWND window, const std::filesystem::path& root);
    void Submit(std::uint64_t requestId, ClipboardImage image);
    std::vector<WorkerEvent> TakeEvents();
    void Stop();

private:
    struct Request {
        std::uint64_t id;
        ClipboardImage image;
        std::chrono::steady_clock::time_point submitted;
    };

    void Run(const std::filesystem::path& root);
    void Send(WorkerEvent event);
    bool IsStopping();

    HWND window_ = nullptr;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_ = false;
    std::optional<Request> pending_;
    std::deque<WorkerEvent> events_;
};

}
