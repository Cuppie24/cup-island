#pragma once
// Scrolling lists on the compositor (decision 029). Each list gets a clipped
// container at its window, and a scroller inside it whose offset eases to
// the list's scroll: the wheel's steps glide instead of jumping, with no app
// code per frame. Its elements sit at their unscrolled place in the scroller,
// and ease to a new one when the list reorders (a drop, a tick); a dragged
// row follows the pointer at once.
//
// A list with buttons floating over it (InList::holes, decision 180) is drawn
// instead through a live snapshot of its scroller, with a soft round hole cut
// round each button: its words fade to nothing there, and whatever the island
// is (glass, a cover's glow) shows through as it is.

#include "core/layout.h"
#include "platform/win.h"

#include <map>
#include <string>
#include <vector>

namespace cup::platform {

class ListViewports {
public:
    void init(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent);

    // Where the element's visual goes: its list's scroller, or the content
    // parent. Creates the list's containers, and moves and scrolls them to
    // match the element (only when that changed, decision 008).
    wuc::ContainerVisual parentFor(const Placed& placed);

    // The element's top-left in its parent's coordinates: in a list, its
    // unscrolled place in the scroller, which scrolling doesn't change.
    static Vec2 positionOf(const Placed& placed);

    // A list element's own ease to a new place (none while dragged).
    void setGliding(wuc::Visual const& visual, bool glide);

    // Lists no element is in any more are removed.
    void prune(const std::vector<Placed>& elements);
    void clear();

private:
    struct List {
        wuc::ContainerVisual clip{ nullptr };     // at the window: clips to it, or (holed) shows `snapshot` with `holes` cut out
        wuc::ContainerVisual scroller{ nullptr }; // holds the elements; offset = -scroll, eased
        // Holed: the scroller in `root`, the snapshot's source, kept in the tree out of sight in `hidden`.
        wuc::ContainerVisual hidden{ nullptr };
        wuc::ContainerVisual root{ nullptr };
        wuc::CompositionVisualSurface snapshot{ nullptr };
        std::vector<wuc::CompositionRadialGradientBrush> holes; // one round each button: clear in it, whole past its fade
        std::size_t asked = 0; // holes asked for when the window was built, built or not (before 1903: none)
        Rect viewport;
        float offset = 0;
        std::vector<Rect> holeFrames;
        float holeFade = 0;
    };

    static std::string keyOf(const Placed& placed) { return placed.activityId + "/" + placed.list->id; }
    // (Re)builds the list's window round its scroller, in the old one's place; then places it.
    void buildWindow(List& list, const InList& in);
    bool holedWindow(List& list, std::size_t holes);
    void placeWindow(List& list, const InList& in);

    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual parent_{ nullptr };
    wuc::ImplicitAnimationCollection scrollEase_{ nullptr }; // the scroller's Offset
    wuc::ImplicitAnimationCollection followEase_{ nullptr }; // ...while it follows a centred row: slower (decision 180)
    wuc::ImplicitAnimationCollection glide_{ nullptr };      // an element's Offset
    std::map<std::string, List> lists_;
};

} // namespace cup::platform
