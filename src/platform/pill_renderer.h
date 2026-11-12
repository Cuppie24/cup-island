#pragma once
// Draws the island: the pill, the Split bubble beside it, the liquid neck
// between them, and a row's open menu over them (decision 086). apply() sets targets and implicit animations do the
// morph (TR §5), so this never redraws per frame.

#include "core/layout.h"
#include "core/motion.h"
#include "core/tokens.h"
#include "platform/blob.h"
#include "platform/graphics_device.h"
#include "platform/liquid_bridge.h"
#include "platform/menu_layer.h"
#include "platform/win.h"

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace cup::platform {

class PillRenderer {
public:
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& root, const Scene& initial);
    GraphicsDevice& device() { return device_; } // shared with providers' windows, on the same Compositor (decision 104)
    // Starts the morph now, along `motion` (decision 080); content follows in a
    // queued call, so building it doesn't hold back the frame the morph starts on (decision 012).
    void apply(const Scene& scene, const SceneMotion& motion);

    // Rebuilds the implicit animations from the current tokens() (after a
    // reload, a theme, a cover's colours). Colours fade; if only colours
    // changed, content is recoloured in place instead of rebuilt.
    void refreshAnimations();

    // A Text's width with the real font, for layout() (core's MeasureText).
    // Cached per string and style; a tokens reload clears it (font sizes).
    float measure(const Text& text);

    // Where a click at `point` (pill coordinates) in a field or an editable
    // Text puts the caret: a byte offset into its text (decisions 043, 045).
    std::size_t caretAt(const Placed& element, Vec2 point);
    // A wrapped field's caret a line up (-1) or down (+1); nothing if there's no line there.
    std::optional<std::size_t> caretOnLine(const Placed& field, int direction);

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown(bool bubble) { (bubble ? bubble_ : pill_).pressDown(); }
    void pressRelease(bool bubble) { (bubble ? bubble_ : pill_).pressRelease(); }

    // Pointer feedback on one of the pill's content elements (see ElementVisuals).
    // A floating toast's elements (keys "toast/...", decision 071) are its own; the rest are the content's.
    void setHovered(const std::string& key)
    {
        bool onToast = isToast(key);
        pill_.elements().setHovered(onToast ? std::string() : key);
        pill_.toastElements().setHovered(onToast ? key : std::string());
    }
    void setHoveredRow(const std::string& row) { pill_.elements().setHoveredRow(row); }
    void setHoveredTab(const std::optional<std::string>& tab) { pill_.elements().setHoveredTab(tab); } // decision 072
    void pressElement(const std::string& key) { elementsFor(key).press(key); }
    void releaseElement(const std::string& key) { elementsFor(key).release(key); }
    void scrubElement(const std::string& key, float fraction) { pill_.elements().scrub(key, fraction); }
    void endScrub(const std::string& key, std::optional<float> seekTo, Ms now) { pill_.elements().endScrub(key, seekTo, now); }

    // Expanded's tabs and Home's tiles: light up the one under the pointer (pill coordinates), or none.
    void setAreaHovered(std::optional<Rect> area, float cornerRadius) { pill_.setHovered(area, cornerRadius); }
    // The keyboard's focus ring (decision 033): around what has focus (pill coordinates), or none.
    void setFocusRing(std::optional<Rect> area, float cornerRadius) { pill_.setFocusRing(area, cornerRadius); }

private:
    void applyContent(); // the latest pending scene's content, for both blobs
    static bool isToast(const std::string& key) { return key.rfind("toast/", 0) == 0; }
    ElementVisuals& elementsFor(const std::string& key) { return isToast(key) ? pill_.toastElements() : pill_.elements(); }

    GraphicsDevice device_;
    Blob pill_;
    Blob bubble_;
    LiquidBridge bridge_;
    MenuLayer menu_; // a row's open menu, over all of it (decision 086)
    std::map<std::pair<std::string, TextStyle>, float> measured_;
    Tokens applied_; // what the visuals were last built or recoloured for: tells a recolour from a resize
    winrt::Windows::System::DispatcherQueue queue_{ nullptr };
    std::optional<Scene> pendingContent_; // set: an applyContent() call is queued
    // When the latest morph that grew the pill is far enough along for new
    // content (decision 080). Kept as a time, not a delay: the content is
    // built later (queued), and that wait counts toward it.
    std::chrono::steady_clock::time_point revealAt_{};
};

} // namespace cup::platform
