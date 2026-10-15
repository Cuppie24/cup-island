#pragma once
// A fake music player: three made-up tracks with gradient "album art", a
// running timeline and working buttons. Exercises the whole content pipeline
// without any real media app; toggled with the M key.

#include "core/provider.h"
#include "providers/media/now_playing.h"

#include <memory>
#include <vector>

namespace cup {

class DemoMediaProvider : public Provider {
public:
    DemoMediaProvider();

    const char* id() const override { return "demo"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override;

    static constexpr const char* kActivityId = "demo.media";

private:
    void changeTrack(int delta);
    void publish();

    struct Track {
        const char* title;
        const char* artist;
        Ms duration;
        std::shared_ptr<const ImageData> art;
        std::shared_ptr<const ImageData> backdrop = nullptr; // art, blurred once (decision 055)
    };
    std::vector<Track> tracks_;
    Host* host_ = nullptr;
    size_t track_ = 0;
    Timeline timeline_;
    bool shuffle_ = false;
    Repeat repeat_ = Repeat::Off;
};

} // namespace cup
