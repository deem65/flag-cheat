#include "RecognitionWorker.h"
#include "ImageDecoder.h"
#include "WindowsSupport.h"
#include "ImageDownload.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace drek_flag_cheat {

    RecognitionWorker::~RecognitionWorker() {
        Stop();
    }

    void RecognitionWorker::Start(HWND window, const std::filesystem::path& root) {
        if (thread_.joinable()) throw std::runtime_error("The recognition worker is already running.");
        window_ = window;
        thread_ = std::thread([this, root] { Run(root); });
    }

    void RecognitionWorker::Submit(std::uint64_t requestId, ClipboardImage image) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;

            pending_ = Request{ requestId, std::move(image), std::chrono::steady_clock::now() };
        }
        condition_.notify_one();
    }

    std::vector<WorkerEvent> RecognitionWorker::TakeEvents() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<WorkerEvent> result;
        result.reserve(events_.size());
        while (!events_.empty()) {
            result.push_back(std::move(events_.front()));
            events_.pop_front();
        }
        return result;
    }

    bool RecognitionWorker::IsStopping() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopping_;
    }

    void RecognitionWorker::Send(WorkerEvent event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            events_.push_back(std::move(event));
        }
        PostMessageW(window_, recognitionEventMessage, 0, 0);
    }

    void RecognitionWorker::Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            pending_.reset();
        }
        condition_.notify_one();
        if (thread_.joinable()) thread_.join();
    }

    void RecognitionWorker::Run(const std::filesystem::path& root) {
        try {
            ComApartment apartment;
            ImageDecoder decoder;
            Matcher matcher;
            LuaPolicy policy(matcher);
            const auto catalog = policy.LoadCatalog(root / "assets" / "flags.lua");
            for (const auto& record : catalog) {
                if (IsStopping()) return;
                try {
                    matcher.AddReference(record, decoder.Read(root / "assets" / "flags" / record.file));
                }
                catch (const std::exception& error) {
                    throw std::runtime_error("Cannot load reference " + record.code + ": " + error.what());
                }
            }
            policy.LoadPolicy(root / "lua" / "recognize.lua");
            WorkerEvent ready;
            ready.kind = WorkerEvent::Kind::Ready;
            ready.referenceCount = matcher.ReferenceCount();
            Send(std::move(ready));

            while (true) {
                Request request;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
                    if (stopping_) return;
                    request = std::move(*pending_);
                    pending_.reset();
                }
                WorkerEvent event;
                event.requestId = request.id;
                try {
                    if (!request.image.imageUrl.empty()) {
                        request.image.encodedPng = DownloadDiscordImage(
                            request.image.imageUrl,
                            [this, activeId = request.id] {
                                std::lock_guard<std::mutex> lock(mutex_);
                                return stopping_ || (pending_ && pending_->id > activeId);
                            });
                    }

                    if (!request.image.encodedPng.empty()) {
                        try {
                            event.preview = decoder.Decode(request.image.encodedPng);
                        }
                        catch (...) {
                            if (!request.image.bitmap) throw;
                            event.preview = std::move(*request.image.bitmap);
                        }
                    }
                    else {
                        event.preview = std::move(request.image.bitmap.value());
                    }
                    event.recognition = policy.Recognize(DescribeImage(event.preview));
                    event.kind = WorkerEvent::Kind::Result;
                    event.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - request.submitted).count();
                }
                catch (const std::exception& error) {
                    event.kind = WorkerEvent::Kind::Error;
                    event.error = error.what();
                }
                Send(std::move(event));
            }
        }
        catch (const std::exception& error) {
            WorkerEvent failure;
            failure.error = error.what();
            Send(std::move(failure)); 
        }
    }

}
