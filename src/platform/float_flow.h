#pragma once
// A floating card flowing out of the island and back (decision 081): a
// copy of the pill, in the island's colours, peels off the top edge as the
// island lifts (decision 026) and springs on the island's spring (decision
// 080) into the card's place, its corners easing to a window's. The card's
// own window takes over at the end. Back is the same, the other way.
// One click-through layer, made the first time, reused for every flow.

#include "platform/blob.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <functional>

namespace cup::platform {

class FloatFlow {
public:
    FloatFlow() = default;
    FloatFlow(const FloatFlow&) = delete;
    FloatFlow& operator=(const FloatFlow&) = delete;
    ~FloatFlow() { destroy(); }

    // From the pill (screen px, hanging from the screen's top edge) to the
    // card's frame, then `done` (show the card's window). A flow already
    // running ends at once, its own `done` first. Reduced motion: `done` now.
    void out(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect pill, Rect card,
             std::function<void()> done);
    // From the card's frame back into the pill, then `done` (drop its window).
    void in(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect card, Rect pill,
            std::function<void()> done);
    void destroy();

private:
    void play(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device, Rect from, Rect to, bool outward,
              std::function<void()> done);
    bool make(HINSTANCE instance, wuc::Compositor const& compositor, GraphicsDevice& device);
    void finish(); // the flow's end: `done`, then the layer goes a moment later

    HWND window_{};
    wuc::Desktop::DesktopWindowTarget target_{ nullptr };
    wuc::ContainerVisual root_{ nullptr };
    Blob blob_;
    bool built_ = false;
    winrt::Windows::System::DispatcherQueueTimer end_{ nullptr }, hide_{ nullptr };
    std::function<void()> done_;
};

} // namespace cup::platform
