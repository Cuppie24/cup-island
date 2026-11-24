#pragma once
// The playing app's own volume, as Windows' volume mixer has it (decision
// 180): read and set through its audio sessions. Windows only.
//
// Threading: every call into the audio service is made on a thread of its
// own, like the media sessions' (decision 144): a call into another process
// that never returns only stops the volume answering. It reports from that
// thread: the provider posts to the UI thread.

#include "platform/win.h" // before any winrt header: the audio interfaces are classic COM

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace cup {

class AppVolume : public std::enable_shared_from_this<AppVolume> {
public:
    struct Level {
        float volume = 1; // 0..1, of the system's volume
        bool muted = false;
    };
    // `app`: the media session's app id, as given to read(). Nothing: the app
    // has no audio session now (it hasn't played yet, or its id names no process).
    using Report = std::function<void(std::string app, std::optional<Level> level)>;

    static std::shared_ptr<AppVolume> start(Report report); // from the UI thread
    void stop();                                            // returns at once; late reports become no-ops

    // `app`: a session's SourceAppUserModelId, UTF-8. Every audio session of that app is meant.
    void read(const std::string& app);            // reported
    void set(const std::string& app, float volume); // ...and unmuted; not reported: the caller knows. The latest of many wins

private:
    explicit AppVolume(Report report) : report_(std::move(report)) {}
    void post(std::function<void()> work);

    Report report_;
    std::atomic<bool> stopped_ = false;
    std::atomic<std::uint64_t> latestSet_ = 0; // a drag queues many: only the last is carried out
    winrt::Windows::System::DispatcherQueueController controller_{ nullptr };
    winrt::Windows::System::DispatcherQueue queue_{ nullptr };
};

} // namespace cup
